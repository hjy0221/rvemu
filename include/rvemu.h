#ifndef RVEMU_H
#define RVEMU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RVEMU_REG_COUNT 32

typedef enum {
    RVEMU_OK = 0,
    RVEMU_HALT,
    RVEMU_ERR_MEMORY,
    RVEMU_ERR_INSTRUCTION,
    RVEMU_ERR_ALIGNMENT,
    RVEMU_ERR_LIMIT
} rvemu_status;

typedef struct {
    uint32_t x[RVEMU_REG_COUNT];
    uint32_t pc;
    uint8_t *memory;
    size_t memory_size;
    bool halted;
    rvemu_status last_status;
} rvemu;

bool rvemu_init(rvemu *cpu, size_t memory_size);
void rvemu_destroy(rvemu *cpu);
rvemu_status rvemu_step(rvemu *cpu);
rvemu_status rvemu_run(rvemu *cpu, uint64_t max_steps, uint64_t *steps_run);
bool rvemu_load_bytes(rvemu *cpu, uint32_t address, const void *data, size_t size);
const char *rvemu_status_string(rvemu_status status);

#endif
