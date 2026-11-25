#include "rvemu.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    uint64_t limit = 1000000;
    if (argc > 3 || (argc >= 2 && strcmp(argv[1], "--help") == 0)) {
        fprintf(argc >= 2 && strcmp(argv[1], "--help") == 0 ? stdout : stderr,
                "Usage: %s [program.bin] [max-steps]\n", argv[0]);
        return argc >= 2 && strcmp(argv[1], "--help") == 0 ? 0 : 2;
    }
    if (argc == 3) {
        char *end = NULL; limit = strtoull(argv[2], &end, 10);
        if (!end || *end || !limit) { fprintf(stderr,"invalid step limit\n"); return 2; }
    }
    rvemu cpu;
    if (!rvemu_init(&cpu, 1024 * 1024)) { fprintf(stderr,"failed to allocate memory\n"); return 1; }
    if (argc >= 2) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { perror(argv[1]); rvemu_destroy(&cpu); return 1; }
        size_t n = fread(cpu.memory, 1, cpu.memory_size, f);
        bool extra = !feof(f);
        if (ferror(f) || extra) { fprintf(stderr,"program is too large or unreadable\n"); fclose(f); rvemu_destroy(&cpu); return 1; }
        fclose(f);
        if (n < 4) { fprintf(stderr,"program must contain at least one instruction\n"); rvemu_destroy(&cpu); return 1; }
    } else {
        /* addi x10,x0,42; ebreak */
        const uint32_t demo[] = { 0x02a00513u, 0x00100073u };
        rvemu_load_bytes(&cpu, 0, demo, sizeof(demo));
    }
    uint64_t steps = 0;
    rvemu_status status = rvemu_run(&cpu, limit, &steps);
    printf("status: %s\nsteps:  %" PRIu64 "\npc:     0x%08" PRIx32 "\nx10:    0x%08" PRIx32 " (%" PRIu32 ")\n",
           rvemu_status_string(status), steps, cpu.pc, cpu.x[10], cpu.x[10]);
    rvemu_destroy(&cpu);
    return status == RVEMU_HALT ? 0 : 1;
}
