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

int main(int argc, const char* argv[]) {
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    /* 
     * 【测试代码】：我们手动在内存 0x3000 处写一段真正的机器码程序！
     * 指令 1：ADD R0, R0, #5   (往 R0 加上立即数 5) -> 0x1025
     * 指令 2：ADD R1, R0, #10  (R1 = R0 + 10 = 15) -> 0x126A
     * 指令 3：ADD R2, R0, R1   (R2 = R0 + R1 = 20) -> 0x1441
     * 指令 4：停机指令
     */
    memory[0x3000] = 0x1025; /* R0 = R0 + 5 */
    memory[0x3001] = 0x126A; /* R1 = R0 + 10 */
    memory[0x3002] = 0x1441; /* R2 = R0 + R1 */
    memory[0x3003] = (OP_TRAP << 12); /* 停机 */

    int running = 1;
    printf("CPU 启动，开始执行测试程序...\n");

    while (running) {
        uint16_t instr = memory[reg[R_PC]++];
        uint16_t op = instr >> 12;

        switch (op) {
            case OP_ADD: {
                /* 目的寄存器 DR (bits 11-9) */
                uint16_t r0 = (instr >> 9) & 0x7;
                /* 第一个源操作数 SR1 (bits 8-6) */
                uint16_t r1 = (instr >> 6) & 0x7;
                /* 判断是否是立即数模式 (bit 5) */
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

                reg[r0] = ~reg[r1]; /* 按位取反 */
                update_flags(r0);
                break;
            }

            case OP_TRAP:
                printf("\n--- 计算完成！查看 CPU 寄存器状态 ---\n");
                printf("R0 寄存器值: %d (预期: 5)\n", reg[R_R0]);
                printf("R1 寄存器值: %d (预期: 15)\n", reg[R_R1]);
                printf("R2 寄存器值: %d (预期: 20)\n", reg[R_R2]);
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