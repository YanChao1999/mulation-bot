#include "catalog.hpp"
#include "unit_check.hpp"

#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

TEST(Catalog, ParseOneRecord) {
    auto ms = parse_ndjson(
        "{\"id\":42,\"file\":\"src/foo.cpp\",\"line\":10,\"col\":3,\"kind\":\"ROR\","
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
    auto ms = parse_ndjson(
        "{\"id\":1,\"file\":\"dir\\\\foo.cpp\",\"line\":1,\"col\":0,\"kind\":\"LVR\","
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
