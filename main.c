#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/termios.h>
#endif

#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];

/* 寄存器定义 */
enum {
    R_R0 = 0, R_R1, R_R2, R_R3, R_R4, R_R5, R_R6, R_R7,
    R_PC, R_COND, R_COUNT
};
uint16_t reg[R_COUNT];

/* 操作码定义 */
enum {
    OP_BR = 0, OP_ADD, OP_LD, OP_ST, OP_JSR, OP_AND, OP_LDR,
    OP_STR, OP_RTI, OP_NOT, OP_LDI, OP_STI, OP_JMP, OP_RES,
    OP_LEA, OP_TRAP
};

/* 条件码定义 */
enum {
    FL_POS = 1 << 0,
    FL_ZRO = 1 << 1,
    FL_NEG = 1 << 2,
};

/* 内存映射寄存器（键盘设备） */
enum {
    MR_KBSR = 0xFE00, /* 键盘状态寄存器：检测是否有按键输入 */
    MR_KBDR = 0xFE02  /* 键盘数据寄存器：存储具体的按键字符 */
};

/* TRAP 系统调用编号 */
enum {
    TRAP_GETC = 0x20,  /* 从键盘读取单个字符（不回显） */
    TRAP_OUT = 0x21,   /* 输出单个字符到控制台 */
    TRAP_PUTS = 0x22,  /* 输出一串字符串 */
    TRAP_IN = 0x23,    /* 提示并接收一个字符（回显） */
    TRAP_PUTSP = 0x24, /* 输出打包字符串 */
    TRAP_HALT = 0x25   /* 停机指令 */
};

/* 运行标志位 */
int running = 1;

/* 符号扩展 */
uint16_t sign_extend(uint16_t x, int bit_count) {
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

/* 大端序与小端序翻转 */
uint16_t swap16(uint16_t x) {
    return (x << 8) | (x >> 8);
}

/* 更新条件标志 */
void update_flags(uint16_t r) {
    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if (reg[r] >> 15) {
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

/* 内存写入 */
void mem_write(uint16_t address, uint16_t val) {
    memory[address] = val;
}

/* 内存读取（包含键盘硬件映射处理） */
uint16_t mem_read(uint16_t address) {
    if (address == MR_KBSR) {
#ifdef _WIN32
        if (_kbhit()) {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = _getch();
        } else {
            memory[MR_KBSR] = 0;
        }
#else
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval timeout = {0, 0};
        if (select(1, &readfds, NULL, NULL, &timeout) != 0) {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        } else {
            memory[MR_KBSR] = 0;
        }
#endif
    }
    return memory[address];
}

/* 从磁盘加载二进制程序镜像文件 */
void read_image_file(FILE* file) {
    /* 1. 读出程序起始加载地址（大端序） */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    /* 2. 连续读取所有指令塞入对应内存地址 */
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* 3. 逐个将指令转换为本机小端序 */
    while (read-- > 0) {
        *p = swap16(*p);
        ++p;
    }
}

int read_image(const char* image_path) {
    FILE* file = fopen(image_path, "rb");
    if (!file) return 0;
    read_image_file(file);
    fclose(file);
    return 1;
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        printf("使用方式: lc3-vm [程序镜像文件.obj]\n");
        printf("未提供文件，进入默认终端欢迎测试程序...\n\n");
        
        /* 内置一个测试程序：输出欢迎字符并停机 */
        reg[R_PC] = 0x3000;
        memory[0x3000] = (OP_LEA << 12) | (R_R0 << 9) | 2;      /* LEA R0, 2 (指向字符串) */
        memory[0x3001] = (OP_TRAP << 12) | TRAP_PUTS;           /* PUTS 输出字符串 */
        memory[0x3002] = (OP_TRAP << 12) | TRAP_HALT;           /* HALT 停机 */
        /* 塞入 "LC-3 VM Ready!\n\0" 的 ASCII 码 */
        const char* msg = ">>> [SYSTEM] LC-3 VM is fully operational! <<<\n";
        for (int i = 0; i <= strlen(msg); i++) {
            memory[0x3003 + i] = msg[i];
        }
    } else {
        if (!read_image(argv[1])) {
            printf("加载程序镜像失败: %s\n", argv[1]);
            return 1;
        }
        reg[R_PC] = 0x3000; /* 默认程序计数器起点 */
    }

    reg[R_COND] = FL_ZRO;

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
            case OP_BR: {
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                uint16_t cond_flag = (instr >> 9) & 0x7;
                if (cond_flag & reg[R_COND]) {
                    reg[R_PC] += pc_offset;
                }
                break;
            }
            case OP_JMP: {
                uint16_t r1 = (instr >> 6) & 0x7;
                reg[R_PC] = reg[r1];
                break;
            }
            case OP_JSR: {
                uint16_t long_flag = (instr >> 11) & 1;
                reg[R_R7] = reg[R_PC];
                if (long_flag) {
                    uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                    reg[R_PC] += long_pc_offset;
                } else {
                    uint16_t r1 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r1];
                }
                break;
            }
            case OP_LD: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(reg[R_PC] + pc_offset);
                update_flags(r0);
                break;
            }
            case OP_LDI: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
                update_flags(r0);
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
            case OP_LEA: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = reg[R_PC] + pc_offset;
                update_flags(r0);
                break;
            }
            case OP_ST: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(reg[R_PC] + pc_offset, reg[r0]);
                break;
            }
            case OP_STI: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
                break;
            }
            case OP_STR: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                mem_write(reg[r1] + offset, reg[r0]);
                break;
            }
            case OP_TRAP: {
                reg[R_R7] = reg[R_PC];
                switch (instr & 0xFF) {
                    case TRAP_GETC:
                        reg[R_R0] = (uint16_t)getchar();
                        update_flags(R_R0);
                        break;
                    case TRAP_OUT:
                        putc((char)reg[R_R0], stdout);
                        fflush(stdout);
                        break;
                    case TRAP_PUTS: {
                        uint16_t* c = memory + reg[R_R0];
                        while (*c) {
                            putc((char)*c, stdout);
                            ++c;
                        }
                        fflush(stdout);
                        break;
                    }
                    case TRAP_IN: {
                        printf("输入字符: ");
                        char c = getchar();
                        putc(c, stdout);
                        fflush(stdout);
                        reg[R_R0] = (uint16_t)c;
                        update_flags(R_R0);
                        break;
                    }
                    case TRAP_PUTSP: {
                        uint16_t* c = memory + reg[R_R0];
                        while (*c) {
                            char c1 = (*c) & 0xFF;
                            putc(c1, stdout);
                            char c2 = (*c) >> 8;
                            if (c2) putc(c2, stdout);
                            ++c;
                        }
                        fflush(stdout);
                        break;
                    }
                    case TRAP_HALT:
                        puts("\n[HALT] 虚拟机系统停机！");
                        running = 0;
                        break;
                }
                break;
            }
            case OP_RES:
            case OP_RTI:
            default:
                abort();
                break;
        }
    }

    return 0;
}