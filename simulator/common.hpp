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

// Bit32がとる型
enum class Type{
    t_int,
    t_float
};

// 整数と浮動小数の共用体を模したクラス
class Bit32{
    public:
        int v;
        Type t;
        Bit32(int i);
        Bit32(float f);
        int to_int();
        float to_float();
        std::string to_string();
};
