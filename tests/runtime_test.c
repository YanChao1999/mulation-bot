#include "mulation/mulation.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: runtime_test <id> <want-0-or-1> [extra-ids...]\n");
        return 2;
    }
    unsigned id = (unsigned)strtoul(argv[1], NULL, 10);
    int want = atoi(argv[2]);
    int got = mulation_active(id) ? 1 : 0;
    if (got != want) {
        fprintf(stderr, "mulation_active(%u)=%d want %d\n", id, got, want);
        return 1;
    }
    for (int i = 3; i < argc; ++i) {
        (void)mulation_active((unsigned)strtoul(argv[i], NULL, 10));
    }
    return 0;
}
