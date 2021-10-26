#pragma once
#include <string>

// 命令
class Operation{
    public:
        Operation(std::string code);
        int opcode;
        int funct;
        int rs1;
        int rs2;
        int rd;
        int imm;
        std::string to_string();
        // std::string to_code()
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

// 文字列変換の際に指定対象となる型
enum class Stype{
    t_default,
    t_dec,
    t_bin,
    t_hex,
    t_float,
    t_op
};

// 整数と浮動小数の共用体を模したクラス
class Bit32{
    public:
        int v; // 内部的にはint
        Type t; // 値の(本当の)型
        Bit32();
        Bit32(int i);
        Bit32(int i, Type t);
        Bit32(float f);
        int to_int();
        float to_float();
        std::string to_string();
        std::string to_string(Stype t);
        std::string to_string(Stype t, const int len);
};
