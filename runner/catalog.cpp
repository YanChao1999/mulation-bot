#include "catalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <elf.h>
#include <fstream>
#include <set>
#include <sstream>

std::string json_escape(const std::string &s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            o += "\\\"";
            break;
        case '\\':
            o += "\\\\";
            break;
        case '\n':
            o += "\\n";
            break;
        case '\r':
            o += "\\r";
            break;
        case '\t':
            o += "\\t";
            break;
        default:
            if (c < 0x20) {
                o += "\\u00";
                const char *hex = "0123456789abcdef";
                o.push_back(hex[c >> 4]);
                o.push_back(hex[c & 0xf]);
            } else {
                o.push_back(static_cast<char>(c));
            }
        }
    }
    return o;
}

std::vector<std::string> parse_ctest_command_paths(const std::string &json) {
    std::vector<std::string> paths;
    const std::string key = "\"command\"";
    std::size_t pos = 0;
    while ((pos = json.find(key, pos)) != std::string::npos) {
        std::size_t i = pos + key.size();
        while (i < json.size() &&
               (std::isspace(static_cast<unsigned char>(json[i])) || json[i] == ':')) {
            ++i;
        }
        if (i >= json.size() || json[i] != '[') {
            pos = i;
            continue;
        }
        ++i;
        while (i < json.size() && json[i] != ']') {
            while (i < json.size() &&
                   (std::isspace(static_cast<unsigned char>(json[i])) || json[i] == ',')) {
                ++i;
            }
            if (i >= json.size() || json[i] == ']') {
                break;
            }
            if (json[i] != '"') {
                ++i;
                continue;
            }
            ++i;
            std::string val;
            while (i < json.size() && json[i] != '"') {
                if (json[i] == '\\' && i + 1 < json.size()) {
                    val.push_back(json[i + 1]);
                    i += 2;
                } else {
                    val.push_back(json[i++]);
                }
            }
            if (i < json.size() && json[i] == '"') {
                ++i;
            }
            if (!val.empty()) {
                paths.push_back(std::move(val));
            }
        }
        pos = i;
    }
    return paths;
}

static std::string json_string_field(const std::string &line, const char *key) {
    std::string pat = std::string("\"") + key + "\":\"";
    auto p = line.find(pat);
    if (p == std::string::npos) {
        return {};
    }
    p += pat.size();
    std::string out;
    while (p < line.size() && line[p] != '"') {
        if (line[p] == '\\' && p + 1 < line.size()) {
            char n = line[++p];
            if (n == 'n') {
                out += '\n';
            } else if (n == 't') {
                out += '\t';
            } else {
                out += n;
            }
        } else {
            out += line[p];
        }
        ++p;
    }
    return out;
}

static unsigned json_uint_field(const std::string &line, const char *key) {
    std::string pat = std::string("\"") + key + "\":";
    auto p = line.find(pat);
    if (p == std::string::npos) {
        return 0;
    }
    p += pat.size();
    return static_cast<unsigned>(std::strtoul(line.c_str() + p, nullptr, 10));
}

std::vector<Mutant> parse_ndjson(const std::string &text) {
    std::vector<Mutant> out;
    std::string line;
    std::istringstream in(text);
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '\0' || line[0] != '{') {
            continue;
        }
        Mutant m;
        m.id = json_uint_field(line, "id");
        m.file = json_string_field(line, "file");
        m.line = json_uint_field(line, "line");
        m.col = json_uint_field(line, "col");
        m.kind = json_string_field(line, "kind");
        m.op = json_string_field(line, "op");
        m.mut = json_string_field(line, "mut");
        if (m.id != 0 && !m.file.empty()) {
            out.push_back(std::move(m));
        }
    }
    return out;
}

static std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<Mutant> read_elf_catalog(const std::string &binary) {
    std::ifstream in(binary, std::ios::binary);
    if (!in) {
        return {};
    }
    Elf64_Ehdr eh{};
    in.read(reinterpret_cast<char *>(&eh), sizeof(eh));
    if (!in || eh.e_ident[EI_MAG0] != ELFMAG0 || eh.e_ident[EI_MAG1] != ELFMAG1 ||
        eh.e_ident[EI_MAG2] != ELFMAG2 || eh.e_ident[EI_MAG3] != ELFMAG3 ||
        eh.e_ident[EI_CLASS] != ELFCLASS64) {
        return {};
    }
    in.seekg(eh.e_shoff);
    std::vector<Elf64_Shdr> sh(eh.e_shnum);
    in.read(reinterpret_cast<char *>(sh.data()),
            static_cast<std::streamsize>(eh.e_shnum * sizeof(Elf64_Shdr)));
    if (!in || eh.e_shstrndx >= sh.size()) {
        return {};
    }
    const Elf64_Shdr &strsh = sh[eh.e_shstrndx];
    std::string shstr(strsh.sh_size, '\0');
    in.seekg(strsh.sh_offset);
    in.read(shstr.data(), static_cast<std::streamsize>(strsh.sh_size));

    std::string blob;
    for (const Elf64_Shdr &s : sh) {
        if (s.sh_name >= shstr.size()) {
            continue;
        }
        const char *nm = shstr.c_str() + s.sh_name;
        if (std::strcmp(nm, "mulation_mutants") != 0) {
            continue;
        }
        std::string chunk(s.sh_size, '\0');
        in.seekg(s.sh_offset);
        in.read(chunk.data(), static_cast<std::streamsize>(s.sh_size));
        blob += chunk;
        blob += '\n';
    }
    return parse_ndjson(blob);
}

std::vector<Mutant> read_catalog_dir(const std::string &dir) {
    std::vector<Mutant> all;
    DIR *d = opendir(dir.c_str());
    if (!d) {
        return all;
    }
    while (dirent *ent = readdir(d)) {
        std::string n = ent->d_name;
        if (n.size() < 8 || n.substr(n.size() - 7) != ".ndjson") {
            continue;
        }
        auto part = parse_ndjson(read_file(dir + "/" + n));
        all.insert(all.end(), part.begin(), part.end());
    }
    closedir(d);
    return all;
}

std::vector<Mutant> merge_mutants(std::vector<std::vector<Mutant>> groups) {
    std::vector<Mutant> all;
    std::set<uint32_t> seen;
    for (auto &g : groups) {
        for (auto &m : g) {
            if (seen.insert(m.id).second) {
                all.push_back(std::move(m));
            }
        }
    }
    std::sort(all.begin(), all.end(), [](const Mutant &a, const Mutant &b) {
        if (a.file != b.file) {
            return a.file < b.file;
        }
        if (a.line != b.line) {
            return a.line < b.line;
        }
        return a.id < b.id;
    });
    return all;
}
