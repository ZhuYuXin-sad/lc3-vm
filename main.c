#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];

enum {
    R_R0 = 0, R_R1, R_R2, R_R3, R_R4, R_R5, R_R6, R_R7,
    R_PC, R_COND, R_COUNT
};
uint16_t reg[R_COUNT];

enum {
    OP_BR = 0, OP_ADD, OP_LD, OP_ST, OP_JSR, OP_AND, OP_LDR,
    OP_STR, OP_RTI, OP_NOT, OP_LDI, OP_STI, OP_JMP, OP_RES,
    OP_LEA, OP_TRAP
};

enum {
    FL_POS = 1 << 0, /* 正数 */
    FL_ZRO = 1 << 1, /* 零 */
    FL_NEG = 1 << 2, /* 负数 */
};

uint16_t sign_extend(uint16_t x, int bit_count) {
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

void update_flags(uint16_t r) {
    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if (reg[r] >> 15) {
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

void mem_write(uint16_t address, uint16_t val) {
    memory[address] = val;
}

uint16_t mem_read(uint16_t address) {
    return memory[address];
}

int main(int argc, const char* argv[]) {
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    /* 
     * 【Day 4+5 测试程序】：用汇编机器码跑一个 while 循环！
     * 逻辑：R0 = 5; while(R0 > 0) { R0--; }
     * 
     * 1. 0x3000: ADD R0, R0, #5    (初始 R0 = 5)
     * 2. 0x3001: ADD R0, R0, #-1   (R0 减 1，更新条件标志位)
     * 3. 0x3002: BRp #-2           (如果计算结果是正数 P，PC 往后倒退 2 步回到 0x3001 继续减！)
     * 4. 0x3003: TRAP (停机)
     */
    memory[0x3000] = 0x1025; /* ADD R0, R0, #5 */
    memory[0x3001] = 0x103F; /* ADD R0, R0, #-1 (补码 -1) */
    memory[0x3002] = 0x02FE; /* BRp offset=-2 (即 0x0200 | 0x00FE) */
    memory[0x3003] = (OP_TRAP << 12);

    int running = 1;
    printf("CPU 启动，开始执行分支循环与内存测试...\n");

    while (running) {
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op) {
            case OP_ADD: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t imm_flag = (instr >> 5) & 0x1;
                if (imm_flag) {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] + imm5;
                } else {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] + reg[r2];
                }
                update_flags(r0);
                break;
            }

            case OP_AND: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t imm_flag = (instr >> 5) & 0x1;
                if (imm_flag) {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] & imm5;
                } else {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] & reg[r2];
                }
                update_flags(r0);
                break;
            }

            case OP_NOT: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                reg[r0] = ~reg[r1];
                update_flags(r0);
                break;
            }

            /* --- 内存读写指令 --- */
            case OP_LD: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(reg[R_PC] + pc_offset);
                update_flags(r0);
                break;
            }
            case OP_ST: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(reg[R_PC] + pc_offset, reg[r0]);
                break;
            }
            case OP_LDI: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
                update_flags(r0);
                break;
            }
            case OP_STI: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
                break;
            }
            case OP_LDR: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                reg[r0] = mem_read(reg[r1] + offset);
                update_flags(r0);
                break;
            }
            case OP_STR: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                mem_write(reg[r1] + offset, reg[r0]);
                break;
            }
            case OP_LEA: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = reg[R_PC] + pc_offset;
                update_flags(r0);
                break;
            }

            /* --- 分支与跳转指令（核心突破！） --- */
            case OP_BR: { /* 条件分支：BR (Branch) */
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                uint16_t cond_flag = (instr >> 9) & 0x7;
                /* 如果当前计算结果的状态命中要求的标志位，修改 PC 实现跳转！ */
                if (cond_flag & reg[R_COND]) {
                    reg[R_PC] += pc_offset;
                }
                break;
            }

            case OP_JMP: { /* 跳转：JMP / RET */
                uint16_t r1 = (instr >> 6) & 0x7;
                reg[R_PC] = reg[r1];
                break;
            }

            case OP_JSR: { /* 子程序/函数调用：JSR */
                uint16_t long_flag = (instr >> 11) & 1;
                reg[R_R7] = reg[R_PC]; /* 把返回地址存进 R7 */
                if (long_flag) {
                    uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                    reg[R_PC] += long_pc_offset; /* JSR */
                } else {
                    uint16_t r1 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r1]; /* JSRR */
                }
                break;
            }

            case OP_TRAP:
                printf("\n--- 循环测试完成！---\n");
                printf("R0 最终数值: %d (通过循环递减直到为0，预期: 0)\n", reg[R_R0]);
                printf("条件状态 COND: 0x%X (此时应处于零状态 FL_ZRO=0x2)\n", reg[R_COND]);
                running = 0;
                break;

            default:
                printf("未实现的指令码: %d\n", op);
                running = 0;
                break;
        }
    }

    return 0;
}