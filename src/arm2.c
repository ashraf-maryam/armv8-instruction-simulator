/*
 * Lab 5 – ARM Instructions Simulator II
 * --------------------------------------
 * Author: Maryam Ashraf
 *
 * Purpose:
 *  - Simulate execution of a simplified ARM processor that supports:
 *      ADD, ADDI, SUBI, LDUR, STUR, B, BL, BR, CBZ, and CBNZ.
 *  - Demonstrate how the ARM stack functions for function calls, recursion,
 *    and local storage using the Stack Pointer (SP).
 *  - Print out register and stack changes after each instruction step.
 *
 * Notes:
 *  - Stack grows downward from a high memory address (STACK_TOP).
 *  - Stack overflow/underflow is checked for safety.
 *  - Program reads instructions from a text file (e.g., sample_prog.txt).
 *  - The user presses Enter after each step to simulate single-stepping.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_REGISTERS 32
#define MAX_MEMORY    4096
#define START_PC      200

#define STACK_TOP          3504
#define STACK_DEPTH_WORDS  128
#define STACK_BASE         (STACK_TOP - (STACK_DEPTH_WORDS-1)*8)

static char MEM[MAX_MEMORY][5][32];

static int X[MAX_REGISTERS];
static int PC;

static int isNumber(const char *s) {
    if (!s || !*s) return 0;
    if (*s == '+' || *s == '-') s++;
    while (*s) { if (!isdigit((unsigned char)*s)) return 0; s++; }
    return 1;
}

static int parseImm(const char *s) { return s ? atoi(s) : 0; }

static int getRegIndx(const char *reg) {
    if (!reg || !*reg) return -1;

    if (strcmp(reg, "XZR") == 0) return 31;
    if (strcmp(reg, "SP")  == 0) return 28;
    if (strcmp(reg, "LR")  == 0) return 30;

    if (reg[0] == 'X' && isdigit((unsigned char)reg[1])) {
        int idx = atoi(reg + 1);
        if (idx >= 0 && idx < MAX_REGISTERS) return idx;
    }

    if (strcmp(reg, "sp") == 0) return 28;
    if (strcmp(reg, "lr") == 0) return 30;
    if (strcmp(reg, "xzr") == 0) return 31;

    return -1;
}

static void memWriteData(int addr, int v) {
    if (addr < 0 || addr >= MAX_MEMORY) return;
    snprintf(MEM[addr][0], 31, "%d", addr);
    snprintf(MEM[addr][1], 31, "%d", v);
}

static int memReadData(int addr, int *out) {
    if (addr < 0 || addr >= MAX_MEMORY) return 0;
    if (isNumber(MEM[addr][1])) { *out = atoi(MEM[addr][1]); return 1; }
    *out = 0;
    return 1;
}

static void formatInstr(int addr, char *out, size_t n) {
    snprintf(out, n, "%s %s%s%s%s%s",
             MEM[addr][1],
             MEM[addr][2],
             MEM[addr][3][0] ? ", " : "", MEM[addr][3],
             MEM[addr][4][0] ? ", " : "", MEM[addr][4]);
}

static void load_file(const char *file_name) {
    const char *TOK = " \t\n\r[],#";
    FILE *fp = fopen(file_name, "r");
    if (!fp) { fprintf(stderr, "Could not open '%s'\n", file_name); exit(1); }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        size_t L = strlen(line);
        if (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[L - 1] = 0;

        char *mem_address = strtok(line, TOK);
        if (!mem_address) continue;

        char *t1 = strtok(NULL, TOK);
        char *t2 = strtok(NULL, TOK);
        char *t3 = strtok(NULL, TOK);
        char *t4 = strtok(NULL, TOK);

        int addr = atoi(mem_address);
        if (addr < 0 || addr >= MAX_MEMORY) {
            fprintf(stderr, "Input address %d out of range\n", addr);
            exit(1);
        }

        snprintf(MEM[addr][0], 31, "%s", mem_address);
        if (t1) snprintf(MEM[addr][1], 31, "%s", t1); else MEM[addr][1][0] = 0;
        if (t2) snprintf(MEM[addr][2], 31, "%s", t2); else MEM[addr][2][0] = 0;
        if (t3) snprintf(MEM[addr][3], 31, "%s", t3); else MEM[addr][3][0] = 0;
        if (t4) snprintf(MEM[addr][4], 31, "%s", t4); else MEM[addr][4][0] = 0;
    }
    fclose(fp);
}

static void init_stack(void) { X[28] = STACK_TOP; }

static int is_stack_addr(int addr) {
    return addr <= STACK_TOP && addr >= STACK_BASE && (addr % 8 == 0);
}

static void check_stack_bounds(int newSP) {
    if (newSP < STACK_BASE) {
        fprintf(stderr, "ERROR: Stack Overflow (SP=%d < base %d)\n", newSP, STACK_BASE);
        exit(2);
    }
    if (newSP > STACK_TOP) {
        fprintf(stderr, "ERROR: Stack Underflow (SP=%d > top %d)\n", newSP, STACK_TOP);
        exit(2);
    }
}

static void print_regs(unsigned changedMask) {
    printf("Registers:");
    for (int r = 0; r < 32; r++) {
        if (changedMask & (1u << r)) {
            if (r == 31) continue;
            if (r == 28) printf("  SP=%d", X[28]);
            else if (r == 30) printf("  LR=%d", X[30]);
            else printf("  X%d=%d", r, X[r]);
        }
    }
    printf("  PC=%d\n", PC);
}

static void print_stack(void) {
    int sp = X[28];
    if (sp > STACK_TOP) sp = STACK_TOP;
    if (sp < STACK_BASE) sp = STACK_BASE;

    printf("Stack:\n");
    int any = 0;
    for (int addr = STACK_TOP; addr >= sp; addr -= 8) {
        if (!is_stack_addr(addr)) continue;
        int v = 0;
        memReadData(addr, &v);
        if (isNumber(MEM[addr][1])) printf("  %5d: %d\n", addr, v);
        else printf("  %5d:\n", addr);
        any = 1;
    }
    if (!any) printf("  (stack empty; SP=%d)\n", X[28]);
}

static void wait_enter(void) {
    printf("Hit enter for next instruction: > ");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != '\r' && c != EOF) {}
}

static void simulate(void) {
    PC = START_PC;

    for (;;) {
        if (PC < 0 || PC >= MAX_MEMORY) break;
        if (!MEM[PC][1][0]) break;
        if (isNumber(MEM[PC][1])) break;

        int curr = PC;
        char *op = MEM[curr][1];
        char *a  = MEM[curr][2];
        char *b  = MEM[curr][3];
        char *c  = MEM[curr][4];
        int nextPC = PC + 4;
        unsigned changed = 0u;

        if (strcmp(op, "ADDI") == 0) {
            int rd = getRegIndx(a), rn = getRegIndx(b), imm = parseImm(c);
            if (rd != 31) { X[rd] = X[rn] + imm; changed |= (1u << rd); }

        } else if (strcmp(op, "SUBI") == 0) {
            int rd = getRegIndx(a), rn = getRegIndx(b), imm = parseImm(c);
            if (rd != 31) {
                int res = X[rn] - imm;
                if (rd == 28) check_stack_bounds(res);
                X[rd] = res;
                changed |= (1u << rd);
            }

        } else if (strcmp(op, "ADD") == 0) {
            int rd = getRegIndx(a), rn = getRegIndx(b), rm = getRegIndx(c);
            if (rd != 31) { X[rd] = X[rn] + X[rm]; changed |= (1u << rd); }

        } else if (strcmp(op, "LDUR") == 0) {
            int rt = getRegIndx(a), rn = getRegIndx(b), imm = parseImm(c);
            int addr = X[rn] + imm, val = 0;
            memReadData(addr, &val);
            if (rt != 31) { X[rt] = val; changed |= (1u << rt); }

        } else if (strcmp(op, "STUR") == 0) {
            int rt = getRegIndx(a), rn = getRegIndx(b), imm = parseImm(c);
            int addr = X[rn] + imm;
            memWriteData(addr, X[rt]);

        } else if (strcmp(op, "B") == 0) {
            nextPC = (a && a[0] == 'X') ? X[getRegIndx(a)] : parseImm(a);

        } else if (strcmp(op, "BL") == 0) {
            X[30] = PC + 4;
            changed |= (1u << 30);
            nextPC = (a && a[0] == 'X') ? X[getRegIndx(a)] : parseImm(a);

        } else if (strcmp(op, "BR") == 0) {
            int rn = getRegIndx(a);
            if (rn == 31) {
                printf("Halting on BR XZR.\n");
                break;
            }
            nextPC = X[rn];

        } else if (strcmp(op, "CBZ") == 0) {
            int rt = getRegIndx(a), target = parseImm(b);
            nextPC = (X[rt] == 0) ? target : (PC + 4);

        } else if (strcmp(op, "CBNZ") == 0) {
            int rt = getRegIndx(a), target = parseImm(b);
            nextPC = (X[rt] != 0) ? target : (PC + 4);

        } else {
            printf("Unknown instruction at %d: %s\n", curr, op);
            break;
        }

        PC = nextPC;

        char pretty[128];
        formatInstr(curr, pretty, sizeof(pretty));
        printf("Instruction executed: %s\n", pretty);
        print_regs(changed | (1u << 28));
        print_stack();
        wait_enter();
    }
}

static void init_all(void) {
    memset(X, 0, sizeof(X));
    for (int i = 0; i < MAX_MEMORY; i++)
        for (int j = 0; j < 5; j++)
            MEM[i][j][0] = 0;
    PC = START_PC;
    init_stack();
}

int main(int argc, char *argv[]) {
    const char *filename = (argc >= 2) ? argv[1] : "examples/sample_prog.txt";
    init_all();
    load_file(filename);
    simulate();
    return 0;
}
