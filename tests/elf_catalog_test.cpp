#include "catalog.hpp"
#include "unit_check.hpp"

#include <cstdlib>
#include <string>

TEST(Catalog, ReadInstrumentedElfWhenProvided) {
    const char *elf = std::getenv("MULATION_TEST_ELF");
    if (!elf || elf[0] == '\0') {
        return; // optional: enabled by make test after the example is built
    }
    auto ms = read_elf_catalog(elf);
    CHECK(!ms.empty());
    bool has_ror = false;
    bool has_aor = false;
    for (const Mutant &m : ms) {
        CHECK(m.id != 0);
        CHECK(!m.file.empty());
        CHECK(m.line != 0);
        if (m.kind == "ROR") {
            has_ror = true;
        }
        if (m.kind == "AOR") {
            has_aor = true;
        }
    }
    CHECK(has_ror);
    CHECK(has_aor);
}
