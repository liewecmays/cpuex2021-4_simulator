#pragma once
#include <string>

// 命令
class Operation{
    public:
        Operation();
        Operation(std::string code);
        Operation(int i);
        int opcode;
        int funct;
        int rs1;
        int rs2;
        int rd;
        int imm;
        std::string to_string();
        // std::string to_code()
};

// 命令の種類
enum class Otype{
    o_add, o_sub, o_sll, o_srl, o_sra, o_and,
    o_fadd, o_fsub, o_fmul, o_fdiv, o_fsqrt,
    o_beq, o_blt, o_ble,
    o_fbeq, o_fblt,
    o_sw, o_si, o_std,
    o_fsw, o_fstd,
    o_addi, o_slli, o_srli, o_srai, o_andi,
    o_lw, o_lre, o_lrd, o_ltf,
    o_flw, o_flrd,
    o_jalr,
    o_jal,
    o_lui, o_auipc,
    o_fmvif, o_fcvtif,
    o_fmvfi, o_fcvtfi
};

std::string string_of_otype(Otype t); // Otypeを文字列に変換


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
