#include "catalog.hpp"
#include "unit_check.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <elf.h>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

TEST(Catalog, JsonEscapeLowControls) {
    CHECK_EQ(json_escape(std::string("\x01\x1f")), std::string("\\u0001\\u001f"));
}

TEST(Catalog, ParseCTestEmptyCommandArray) {
    auto paths = parse_ctest_command_paths("{\"tests\":[{\"command\":[]}]}");
    CHECK(paths.empty());
}

TEST(Catalog, ParseCTestNoCommandKey) {
    auto paths = parse_ctest_command_paths("{\"tests\":[{\"name\":\"only\"}]}");
    CHECK(paths.empty());
}

TEST(Catalog, JsonEscapeQuotesAndControls) {
    CHECK_EQ(json_escape("a\"b\\c"), std::string("a\\\"b\\\\c"));
    CHECK_EQ(json_escape("x\ny\tz"), std::string("x\\ny\\tz"));
}

TEST(Catalog, ParseCTestCommandPaths) {
    const char *json =
        "{\"tests\":[{\"name\":\"t\",\"command\":[\"/tmp/foo_tests\",\"--gtest_filter=*\"]},"
        "{\"command\" : [ \"/tmp/bar_tests\" ]}]}";
    auto paths = parse_ctest_command_paths(json);
    CHECK_EQ(paths.size(), 3u);
    CHECK_EQ(paths[0], std::string("/tmp/foo_tests"));
    CHECK_EQ(paths[1], std::string("--gtest_filter=*"));
    CHECK_EQ(paths[2], std::string("/tmp/bar_tests"));
}

TEST(Catalog, ParseOneRecord) {
    auto ms =
        parse_ndjson("{\"id\":42,\"file\":\"src/foo.cpp\",\"line\":10,\"col\":3,\"kind\":\"ROR\","
                     "\"op\":\">=\",\"mut\":\">\"}\n");
    CHECK_EQ(ms.size(), 1u);
    CHECK_EQ(ms[0].id, 42u);
    CHECK_EQ(ms[0].file, std::string("src/foo.cpp"));
    CHECK_EQ(ms[0].line, 10u);
    CHECK_EQ(ms[0].col, 3u);
    CHECK_EQ(ms[0].kind, std::string("ROR"));
    CHECK_EQ(ms[0].op, std::string(">="));
    CHECK_EQ(ms[0].mut, std::string(">"));
}

TEST(Catalog, ParseSkipsGarbageAndEmpty) {
    auto ms = parse_ndjson("not json\n\n{\"id\":0,\"file\":\"x.cpp\",\"line\":1}\n"
                           "{\"id\":7,\"file\":\"a.cpp\",\"line\":2,\"col\":1,\"kind\":\"AOR\","
                           "\"op\":\"+\",\"mut\":\"-\"}\n");
    CHECK_EQ(ms.size(), 1u);
    CHECK_EQ(ms[0].id, 7u);
}

TEST(Catalog, ParseEscapedPath) {
    auto ms =
        parse_ndjson("{\"id\":1,\"file\":\"dir\\\\foo.cpp\",\"line\":1,\"col\":0,\"kind\":\"LVR\","
                     "\"op\":\"0\",\"mut\":\"1\"}\n");
    CHECK_EQ(ms.size(), 1u);
    CHECK_EQ(ms[0].file, std::string("dir\\foo.cpp"));
}

TEST(Catalog, MergeDedupsAndSorts) {
    Mutant a{10, "b.cpp", 2, 0, "ROR", ">", ">="};
    Mutant b{10, "a.cpp", 1, 0, "ROR", ">", ">="};
    Mutant c{11, "a.cpp", 1, 0, "AOR", "+", "-"};
    auto merged = merge_mutants({{a}, {b, c}});
    CHECK_EQ(merged.size(), 2u);
    CHECK_EQ(merged[0].id, 11u);
    CHECK_EQ(merged[0].file, std::string("a.cpp"));
    CHECK_EQ(merged[1].id, 10u);
    CHECK_EQ(merged[1].file, std::string("b.cpp"));
}

TEST(Catalog, MergeEmpty) {
    auto merged = merge_mutants({{}, {}});
    CHECK(merged.empty());
}

TEST(Catalog, ReadDirNdjson) {
    char tmpl[] = "/tmp/mulation-cat-XXXXXX";
    char *dir = mkdtemp(tmpl);
    CHECK(dir != nullptr);
    if (!dir) {
        return;
    }
    {
        std::ofstream out(std::string(dir) + "/one.ndjson");
        out << "{\"id\":3,\"file\":\"x.c\",\"line\":8,\"col\":1,\"kind\":\"AOR\","
               "\"op\":\"+\",\"mut\":\"-\"}\n";
    }
    {
        std::ofstream skip(std::string(dir) + "/notes.txt");
        skip << "ignore me\n";
    }
    auto ms = read_catalog_dir(dir);
    CHECK_EQ(ms.size(), 1u);
    CHECK_EQ(ms[0].id, 3u);
}

TEST(Catalog, MissingElfIsEmpty) {
    auto ms = read_elf_catalog("/no/such/binary");
    CHECK(ms.empty());
}

TEST(Catalog, NonElfIsEmpty) {
    char tmpl[] = "/tmp/mulation-notelf-XXXXXX";
    int fd = mkstemp(tmpl);
    CHECK(fd >= 0);
    if (fd >= 0) {
        close(fd);
        std::ofstream out(tmpl);
        out << "not an elf\n";
        auto ms = read_elf_catalog(tmpl);
        CHECK(ms.empty());
        unlink(tmpl);
    }
}

TEST(Catalog, ParseDiscoverCmdPaths) {
    auto paths = parse_discover_cmd_paths("# comment\n  /tmp/a.so \n\n/tmp/b.exe\n");
    CHECK_EQ(paths.size(), 2u);
    CHECK_EQ(paths[0], std::string("/tmp/a.so"));
    CHECK_EQ(paths[1], std::string("/tmp/b.exe"));
}

TEST(Catalog, DiscoverLinkedOnNonElfIsEmpty) {
    auto paths = discover_linked_mutant_elfs("/no/such/binary");
    CHECK(paths.empty());
}

TEST(Catalog, ReadElf32CatalogSection) {
    // Minimal little-endian ELF32 with .shstrtab + mulation_mutants PROGBITS.
    const char payload[] = "{\"id\":99,\"file\":\"sut.c\",\"line\":4,\"col\":1,\"kind\":\"ROR\","
                           "\"op\":\">=\",\"mut\":\">\"}\n";
    const char shstr[] = "\0.shstrtab\0mulation_mutants";
    const uint32_t payload_size = static_cast<uint32_t>(sizeof(payload) - 1);
    const uint32_t shstr_off = sizeof(Elf32_Ehdr);
    const uint32_t payload_off = shstr_off + sizeof(shstr);
    const uint32_t shoff = payload_off + payload_size;

    Elf32_Ehdr eh{};
    eh.e_ident[EI_MAG0] = ELFMAG0;
    eh.e_ident[EI_MAG1] = ELFMAG1;
    eh.e_ident[EI_MAG2] = ELFMAG2;
    eh.e_ident[EI_MAG3] = ELFMAG3;
    eh.e_ident[EI_CLASS] = ELFCLASS32;
    eh.e_ident[EI_DATA] = ELFDATA2LSB;
    eh.e_ident[EI_VERSION] = EV_CURRENT;
    eh.e_type = ET_REL;
    eh.e_machine = EM_386;
    eh.e_version = EV_CURRENT;
    eh.e_ehsize = sizeof(Elf32_Ehdr);
    eh.e_shentsize = sizeof(Elf32_Shdr);
    eh.e_shnum = 3;
    eh.e_shstrndx = 1;
    eh.e_shoff = shoff;

    Elf32_Shdr nullsh{};
    Elf32_Shdr strsh{};
    strsh.sh_name = 1; // ".shstrtab"
    strsh.sh_type = SHT_STRTAB;
    strsh.sh_offset = shstr_off;
    strsh.sh_size = sizeof(shstr);
    Elf32_Shdr mutsh{};
    mutsh.sh_name = 11; // "mulation_mutants"
    mutsh.sh_type = SHT_PROGBITS;
    mutsh.sh_offset = payload_off;
    mutsh.sh_size = payload_size;

    char tmpl[] = "/tmp/mulation-elf32-XXXXXX";
    int fd = mkstemp(tmpl);
    CHECK(fd >= 0);
    if (fd < 0) {
        return;
    }
    close(fd);
    {
        std::ofstream out(tmpl, std::ios::binary);
        out.write(reinterpret_cast<const char *>(&eh), sizeof(eh));
        out.write(shstr, sizeof(shstr));
        out.write(payload, static_cast<std::streamsize>(payload_size));
        out.write(reinterpret_cast<const char *>(&nullsh), sizeof(nullsh));
        out.write(reinterpret_cast<const char *>(&strsh), sizeof(strsh));
        out.write(reinterpret_cast<const char *>(&mutsh), sizeof(mutsh));
    }
    auto ms = read_elf_catalog(tmpl);
    unlink(tmpl);
    CHECK_EQ(ms.size(), 1u);
    CHECK_EQ(ms[0].id, 99u);
    CHECK_EQ(ms[0].file, std::string("sut.c"));
    CHECK_EQ(ms[0].kind, std::string("ROR"));
}
