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
    FL_POS = 1 << 0,
    FL_ZRO = 1 << 1,
    FL_NEG = 1 << 2,
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

/* 
 * 内存读写接口：
 * 计组核心：为什么不直接用 memory[address]？
 * 因为很多外设（如键盘、屏幕）被映射到了特定内存地址，后面做输入输出必须走这个统一接口。
 */
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
     * 【Day 4 测试程序】：测试内存存取
     * 1. 0x3000: ADD R0, R0, #12    (先让 R0 = 12)
     * 2. 0x3001: ST  R0, #2         (把 R0 的值存到当前 PC+2 的内存位置，即 0x3004)
     * 3. 0x3002: LD  R1, #1         (从当前 PC+1 的内存位置读出来，放进 R1)
     * 4. 0x3003: TRAP (停机)
     * 5. 0x3004: [预留存储位置]
     */
    memory[0x3000] = 0x102C; /* ADD R0, R0, #12 */
    memory[0x3001] = 0x3002; /* ST R0, offset=2 -> 存入 0x3004 */
    memory[0x3002] = 0x2201; /* LD R1, offset=1 -> 从 0x3004 读取 */
    memory[0x3003] = (OP_TRAP << 12);

    int running = 1;
    printf("CPU 启动，开始执行内存存取测试...\n");

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

            /* --- 今天新增：内存访问指令 --- */

            case OP_LD: { /* 相对加载：Load (PC-relative) */
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(reg[R_PC] + pc_offset);
                update_flags(r0);
                break;
            }

            case OP_ST: { /* 相对存储：Store (PC-relative) */
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(reg[R_PC] + pc_offset, reg[r0]);
                break;
            }

            case OP_LDI: { /* 间接加载：Load Indirect（二级指针） */
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
                update_flags(r0);
                break;
            }

            case OP_STI: { /* 间接存储：Store Indirect */
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
                break;
            }

            case OP_LDR: { /* 基址偏移加载：Load Base+Offset (类似于 struct/数组) */
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                reg[r0] = mem_read(reg[r1] + offset);
                update_flags(r0);
                break;
            }

            case OP_STR: { /* 基址偏移存储：Store Base+Offset */
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                mem_write(reg[r1] + offset, reg[r0]);
                break;
            }

            case OP_LEA: { /* 加载有效地址：Load Effective Address (C 语言的 & 取地址) */
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = reg[R_PC] + pc_offset;
                update_flags(r0);
                break;
            }

            case OP_TRAP:
                printf("\n--- 测试完成！查看内存与寄存器状态 ---\n");
                printf("内存 0x3004 处的数据: %d (通过 ST 存入，预期: 12)\n", memory[0x3004]);
                printf("R1 寄存器的读取结果: %d (通过 LD 读出，预期: 12)\n", reg[R_R1]);
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