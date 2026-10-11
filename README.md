# ARMv8 Instruction Simulator

A C-based simulator for a simplified ARMv8-style processor. The project models register state, memory, control flow, function calls, stack behavior, and recursion while showing execution state step by step.

## Supported instructions

- `ADD`
- `ADDI`
- `SUBI`
- `LDUR`
- `STUR`
- `B`
- `BL`
- `BR`
- `CBZ`
- `CBNZ`

## Architecture modeled

- 32 general-purpose register slots
- `SP` mapped to `X28`
- `LR` mapped to `X30`
- `XZR` mapped to `X31`
- program counter tracking
- simulated memory for instructions and data
- downward-growing stack with bounds checking
- return-address save/restore behavior for function calls

The simulator prints register and stack state after each executed instruction and supports single-step execution from the terminal.

## Build

### GCC

```bash
gcc -std=c11 -Wall -Wextra -pedantic src/arm2.c -o armv8-sim
```

### CMake

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./armv8-sim examples/sample_prog.txt
```

Press Enter after each instruction to step through execution.

## Example programs

### `sample_prog.txt`

Exercises arithmetic, conditional branches, a function call with `BL`, stack storage of `LR`, and return with `BR`.

### `rec_sum.txt`

Exercises a recursive `sum(n)` routine. It saves and restores `X30` on the stack around recursive calls, demonstrating stack-pointer movement and nested return-address handling.

## Validation

The simulator was compiled with GCC using C11 and tested with the included sample program. The program executed through the call/return sequence and halted correctly on `BR XZR`.

## Tech

C • ARMv8/ARM64 concepts • GCC • CMake • registers • stack memory • control flow • recursion • instruction simulation
