#include "mulation/mulation.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_active = (unsigned)-1;
static unsigned g_hitlog_on;
static char g_hitlog_path[1024];

enum { HIT_CAP = 1 << 16 };

static unsigned g_hit_slots[HIT_CAP];
static unsigned g_hit_count;

static unsigned hit_hash(unsigned id) {
    id ^= id >> 16;
    id *= 0x7feb352du;
    id ^= id >> 15;
    return id;
}

static void record_hit(unsigned id) {
    if (!g_hitlog_on) {
        return;
    }
    unsigned slot = hit_hash(id) & (HIT_CAP - 1);
    for (unsigned n = 0; n < 16; ++n) {
        unsigned i = (slot + n) & (HIT_CAP - 1);
        if (g_hit_slots[i] == 0) {
            g_hit_slots[i] = id;
            ++g_hit_count;
            return;
        }
        if (g_hit_slots[i] == id) {
            return;
        }
    }
}

static void mulation_flush_hits(void) {
    if (!g_hitlog_on || g_hitlog_path[0] == '\0') {
        return;
    }
    FILE *f = fopen(g_hitlog_path, "ab");
    if (!f) {
        return;
    }
    for (unsigned i = 0; i < HIT_CAP; ++i) {
        if (g_hit_slots[i] != 0) {
            fprintf(f, "%u\n", g_hit_slots[i]);
        }
    }
    fclose(f);
}

static void mulation_init(void) __attribute__((constructor));
static void mulation_init(void) {
    const char *e = getenv("MULATION_MUTANT");
    if (e && e[0] != '\0') {
        g_active = (unsigned)strtoul(e, NULL, 10);
    }
    const char *h = getenv("MULATION_HITLOG");
    if (h && h[0] != '\0') {
        g_hitlog_on = 1;
        strncpy(g_hitlog_path, h, sizeof(g_hitlog_path) - 1);
        atexit(mulation_flush_hits);
    }
}

int mulation_active(unsigned id) {
    record_hit(id);
    return g_active == id;
}
