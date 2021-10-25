#pragma once
#include <string>

// 命令
struct Operation{
    int opcode;
    int funct;
    int rs1;
    int rs2;
    int rd;
    int imm;
};

// 整数と浮動小数点数の共用体 (todo: 仕様に合わせる)
union Int_float{
    int i;
    float f;
};
