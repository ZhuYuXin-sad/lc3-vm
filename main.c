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
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

/* 
 * 辅助函数 1：符号扩展（Sign Extend）
 * 计组核心：把一个 5 位或 9 位的二进制补码，扩展成 16 位的标准正负整数
 */
uint16_t sign_extend(uint16_t x, int bit_count) {
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

/* 
 * 辅助函数 2：更新条件标志位（Condition Flags）
 * 每次寄存器计算出结果后，CPU 都要记录它是正数、零还是负数
 */
void update_flags(uint16_t r) {
    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if (reg[r] >> 15) { /* 最高位是 1 说明是负数 */
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

int main(int argc, const char* argv[]) {
    /* 1. 设置程序起始地址：LC-3 的默认起始位置是 0x3000 */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    /* 2. 模拟在内存 0x3000 处塞入一条测试停机指令（模拟 TRAP） */
    memory[PC_START] = (OP_TRAP << 12); 

    int running = 1;
    printf("CPU 启动，进入指令执行主循环...\n");

    /* 3. CPU 指令周期大循环（Fetch -> Decode -> Execute） */
    while (running) {
        /* 取指（FETCH）：从 PC 指向的内存读指令，读完 PC 自动加 1 */
        uint16_t instr = memory[reg[R_PC]++];

        /* 译码（DECODE）：每条指令 16 位，前 4 位就是操作码（右移 12 位拿到） */
        uint16_t op = instr >> 12;

        /* 执行（EXECUTE）：根据操作码分发逻辑 */
        switch (op) {
            case OP_ADD:
                /* 明天实现具体加法 */
                break;
            case OP_TRAP:
                printf("检测到停机指令，CPU 正常关机！\n");
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