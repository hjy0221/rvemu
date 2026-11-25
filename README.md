# rvemu

Documentation: **English** | [한국어](README.ko.md) | [日本語](README.ja.md)

`rvemu` is a small, dependency-free C interpreter for the RV32I base integer instruction set. It is intended as a readable starting point for emulator work, not a complete system emulator.

## Build and run

```sh
make
make test
./build/rvemu                 # runs a tiny built-in example
./build/rvemu program.bin 100000
```

Raw binaries are loaded at address zero and interpreted as little-endian 32-bit instructions. `EBREAK` ends execution. The default step limit prevents a broken guest program from running forever. The current memory is 1 MiB and there is no ELF loader, operating system, device model, or privilege mode.

## Implemented instructions

The interpreter supports RV32I `LUI`, `AUIPC`, `JAL`, `JALR`, all six conditional branches, byte/halfword/word loads and stores (`LB/LH/LW/LBU/LHU`, `SB/SH/SW`), integer register and immediate arithmetic/logical/shift/compare operations, and `FENCE` as a single-hart no-op. `EBREAK` is treated as a host-side halt convention. Unsupported instructions report an error. Memory accesses are little-endian and bounds checked; misaligned data accesses are allowed, while instruction fetch requires 4-byte alignment. Register `x0` is reset to zero after every successful instruction.

## Design

The public `rvemu` state is in `include/rvemu.h`. `src/rvemu.c` owns memory, instruction fetch, decode, immediate sign extension, execution, and status reporting. The step function executes one instruction; the run function adds a configurable instruction budget. This split keeps the execution core usable by tests and future front ends.

## Next extensions

Useful next steps are richer diagnostics and instruction tests, ELF32 loading, configurable memory regions and devices, machine-mode CSRs and traps, an MMU, and finally a Linux-capable platform. A JIT should come after measuring the interpreter and formalizing exception and memory semantics.
