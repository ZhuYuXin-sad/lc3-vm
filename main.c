#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* 
 * 1. 模拟内存：LC-3 有 65536 个内存位置，每个位置存储一个 16 位的值
 * 计算机组成原理：相当于一个 128KB 大小的内存条
 */
#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];

/* 
 * 2. 模拟寄存器：
 * 8个通用寄存器(R0-R7)、1个程序计数器(PC)、1个条件标志寄存器(COND)
 */
enum {
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,    /* 存放下一条要执行的指令地址 */
    R_COND,  /* 存放上一条计算结果的状态（正数、负数、零） */
    R_COUNT
};
uint16_t reg[R_COUNT];

/* 
 * 3. 模拟指令操作码（Opcode）：
 * LC-3 CPU 总共只有 16 条基本指令！
 */
enum {
    OP_BR = 0, /* 分支跳转（Branch） */
    OP_ADD,    /* 加法（Add） */
    OP_LD,     /* 加载数据（Load） */
    OP_ST,     /* 存储数据（Store） */
    OP_JSR,    /* 跳转子程序（Jump register） */
    OP_AND,    /* 按位与（Bitwise and） */
    OP_LDR,    /* 间接加载（Load register） */
    OP_STR,    /* 间接存储（Store register） */
    OP_RTI,    /* 未使用 */
    OP_NOT,    /* 按位非（Bitwise not） */
    OP_LDI,    /* 间接寻址加载 */
    OP_STI,    /* 间接寻址存储 */
    OP_JMP,    /* 跳转（Jump） */
    OP_RES,    /* 保留指令 */
    OP_LEA,    /* 加载有效地址（Load effective address） */
    OP_TRAP    /* 系统调用陷阱（输入输出） */
};

/* 
 * 4. 条件标志位（Condition Flags）
 */
enum {
    FL_POS = 1 << 0, /* 正数标志 P (001) */
    FL_ZRO = 1 << 1, /* 零标志 Z   (010) */
    FL_NEG = 1 << 2, /* 负数标志 N (100) */
};

int main(int argc, const char* argv[]) {
    printf("LC-3 硬件模型初始化完成！\n");
    return 0;
}