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
#include <unistd.h>
#include <vector>

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

template <typename Ehdr, typename Shdr>
static std::vector<Mutant> read_elf_catalog_typed(std::ifstream &in, const Ehdr &eh) {
    in.seekg(static_cast<std::streamoff>(eh.e_shoff));
    std::vector<Shdr> sh(eh.e_shnum);
    in.read(reinterpret_cast<char *>(sh.data()),
            static_cast<std::streamsize>(eh.e_shnum * sizeof(Shdr)));
    if (!in || eh.e_shstrndx >= sh.size()) {
        return {};
    }
    const Shdr &strsh = sh[eh.e_shstrndx];
    std::string shstr(static_cast<std::size_t>(strsh.sh_size), '\0');
    in.seekg(static_cast<std::streamoff>(strsh.sh_offset));
    in.read(shstr.data(), static_cast<std::streamsize>(strsh.sh_size));

    std::string blob;
    for (const Shdr &s : sh) {
        if (s.sh_name >= shstr.size()) {
            continue;
        }
        const char *nm = shstr.c_str() + s.sh_name;
        if (std::strcmp(nm, "mulation_mutants") != 0) {
            continue;
        }
        std::string chunk(static_cast<std::size_t>(s.sh_size), '\0');
        in.seekg(static_cast<std::streamoff>(s.sh_offset));
        in.read(chunk.data(), static_cast<std::streamsize>(s.sh_size));
        blob += chunk;
        blob += '\n';
    }
    return parse_ndjson(blob);
}

std::vector<Mutant> read_elf_catalog(const std::string &binary) {
    std::ifstream in(binary, std::ios::binary);
    if (!in) {
        return {};
    }
    unsigned char ident[EI_NIDENT]{};
    in.read(reinterpret_cast<char *>(ident), EI_NIDENT);
    if (!in || ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
        ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3) {
        return {};
    }
    in.seekg(0);
    if (ident[EI_CLASS] == ELFCLASS64) {
        Elf64_Ehdr eh{};
        in.read(reinterpret_cast<char *>(&eh), sizeof(eh));
        if (!in) {
            return {};
        }
        return read_elf_catalog_typed<Elf64_Ehdr, Elf64_Shdr>(in, eh);
    }
    if (ident[EI_CLASS] == ELFCLASS32) {
        Elf32_Ehdr eh{};
        in.read(reinterpret_cast<char *>(&eh), sizeof(eh));
        if (!in) {
            return {};
        }
        return read_elf_catalog_typed<Elf32_Ehdr, Elf32_Shdr>(in, eh);
    }
    return {};
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

namespace {

std::string dirname_of_path(std::string path) {
    auto slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return "/";
    }
    return path.substr(0, slash);
}

std::string expand_origin(std::string token, const std::string &origin) {
    const std::string key = "$ORIGIN";
    std::size_t pos = 0;
    while ((pos = token.find(key, pos)) != std::string::npos) {
        token.replace(pos, key.size(), origin);
        pos += origin.size();
    }
    return token;
}

std::vector<std::string> split_colon(const std::string &s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ':') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

template <typename Ehdr, typename Phdr, typename Dyn>
static bool read_dynamic_needed(std::ifstream &in, const Ehdr &eh, std::vector<std::string> &needed,
                                std::string &runpath, std::string &rpath) {
    if (eh.e_phoff == 0 || eh.e_phnum == 0) {
        return false;
    }
    in.seekg(static_cast<std::streamoff>(eh.e_phoff));
    std::vector<Phdr> ph(eh.e_phnum);
    in.read(reinterpret_cast<char *>(ph.data()),
            static_cast<std::streamsize>(eh.e_phnum * sizeof(Phdr)));
    if (!in) {
        return false;
    }
    const Phdr *dynph = nullptr;
    for (const Phdr &p : ph) {
        if (p.p_type == PT_DYNAMIC) {
            dynph = &p;
            break;
        }
    }
    if (!dynph || dynph->p_filesz < sizeof(Dyn)) {
        return false;
    }
    const std::size_t ndyn = static_cast<std::size_t>(dynph->p_filesz / sizeof(Dyn));
    std::vector<Dyn> dyn(ndyn);
    in.seekg(static_cast<std::streamoff>(dynph->p_offset));
    in.read(reinterpret_cast<char *>(dyn.data()), static_cast<std::streamsize>(dynph->p_filesz));
    if (!in) {
        return false;
    }

    uint64_t strtab_vaddr = 0;
    uint64_t strsz = 0;
    std::vector<uint64_t> needed_offs;
    for (const Dyn &d : dyn) {
        if (d.d_tag == DT_NULL) {
            break;
        }
        if (d.d_tag == DT_STRTAB) {
            strtab_vaddr = static_cast<uint64_t>(d.d_un.d_ptr);
        } else if (d.d_tag == DT_STRSZ) {
            strsz = static_cast<uint64_t>(d.d_un.d_val);
        } else if (d.d_tag == DT_NEEDED) {
            needed_offs.push_back(static_cast<uint64_t>(d.d_un.d_val));
        } else if (d.d_tag == DT_RUNPATH) {
            // resolved after we have strtab
        } else if (d.d_tag == DT_RPATH) {
            // resolved after we have strtab
        }
    }
    if (strtab_vaddr == 0 || strsz == 0) {
        return false;
    }

    // Map virtual address of strtab to file offset via PT_LOAD.
    uint64_t strtab_off = 0;
    bool mapped = false;
    for (const Phdr &p : ph) {
        if (p.p_type != PT_LOAD) {
            continue;
        }
        const uint64_t vaddr = static_cast<uint64_t>(p.p_vaddr);
        const uint64_t filesz = static_cast<uint64_t>(p.p_filesz);
        if (strtab_vaddr >= vaddr && strtab_vaddr < vaddr + filesz) {
            strtab_off = static_cast<uint64_t>(p.p_offset) + (strtab_vaddr - vaddr);
            mapped = true;
            break;
        }
    }
    if (!mapped) {
        return false;
    }
    std::string strtab(static_cast<std::size_t>(strsz), '\0');
    in.seekg(static_cast<std::streamoff>(strtab_off));
    in.read(strtab.data(), static_cast<std::streamsize>(strsz));
    if (!in) {
        return false;
    }

    auto at = [&](uint64_t off) -> std::string {
        if (off >= strtab.size()) {
            return {};
        }
        return std::string(strtab.c_str() + off);
    };
    for (const Dyn &d : dyn) {
        if (d.d_tag == DT_NULL) {
            break;
        }
        if (d.d_tag == DT_RUNPATH) {
            runpath = at(static_cast<uint64_t>(d.d_un.d_val));
        } else if (d.d_tag == DT_RPATH) {
            rpath = at(static_cast<uint64_t>(d.d_un.d_val));
        }
    }
    for (uint64_t off : needed_offs) {
        std::string n = at(off);
        if (!n.empty()) {
            needed.push_back(std::move(n));
        }
    }
    return !needed.empty();
}

static std::string resolve_needed(const std::string &soname, const std::string &origin,
                                  const std::string &runpath, const std::string &rpath) {
    std::vector<std::string> dirs;
    dirs.push_back(origin);
    for (const std::string &tok : split_colon(runpath.empty() ? rpath : runpath)) {
        dirs.push_back(expand_origin(tok, origin));
    }
    for (const std::string &dir : dirs) {
        if (dir.empty()) {
            continue;
        }
        std::string cand = dir;
        if (cand.back() != '/') {
            cand.push_back('/');
        }
        cand += soname;
        if (access(cand.c_str(), F_OK) == 0) {
            return cand;
        }
    }
    return {};
}

} // namespace

std::vector<std::string> discover_linked_mutant_elfs(const std::string &binary) {
    std::vector<std::string> out;
    std::ifstream in(binary, std::ios::binary);
    if (!in) {
        return out;
    }
    unsigned char ident[EI_NIDENT]{};
    in.read(reinterpret_cast<char *>(ident), EI_NIDENT);
    if (!in || ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
        ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3) {
        return out;
    }
    in.seekg(0);
    std::vector<std::string> needed;
    std::string runpath;
    std::string rpath;
    if (ident[EI_CLASS] == ELFCLASS64) {
        Elf64_Ehdr eh{};
        in.read(reinterpret_cast<char *>(&eh), sizeof(eh));
        if (!in || !read_dynamic_needed<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn>(in, eh, needed, runpath,
                                                                           rpath)) {
            return out;
        }
    } else if (ident[EI_CLASS] == ELFCLASS32) {
        Elf32_Ehdr eh{};
        in.read(reinterpret_cast<char *>(&eh), sizeof(eh));
        if (!in || !read_dynamic_needed<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn>(in, eh, needed, runpath,
                                                                           rpath)) {
            return out;
        }
    } else {
        return out;
    }

    const std::string origin = dirname_of_path(binary);
    std::set<std::string> seen;
    for (const std::string &soname : needed) {
        std::string path = resolve_needed(soname, origin, runpath, rpath);
        if (path.empty() || !seen.insert(path).second) {
            continue;
        }
        if (!read_elf_catalog(path).empty()) {
            out.push_back(std::move(path));
        }
    }
    return out;
}

std::vector<std::string> parse_discover_cmd_paths(const std::string &stdout_text) {
    std::vector<std::string> out;
    std::istringstream in(stdout_text);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() &&
               (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        std::size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
            ++start;
        }
        if (start > 0) {
            line = line.substr(start);
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        out.push_back(std::move(line));
    }
    return out;
}
