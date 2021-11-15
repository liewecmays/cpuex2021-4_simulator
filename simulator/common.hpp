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
inline constexpr int op_type_num = 41;
enum Otype{
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
    o_fmvfi, o_fcvtfi, o_floor
};

std::string string_of_otype(Otype t); // Otypeを文字列に変換


// 内部表現を考慮したfloat
struct Float{
    unsigned int m : 23;
    unsigned int e : 8;
    unsigned int s : 1;
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

// 32ビット列を扱うための共用体
union Bit32{
    int i;
    unsigned int ui;
    float f;
    Float F;
    Bit32();
    Bit32(int i);
    Bit32(unsigned int ui);
    Bit32(float f);
    std::string to_string();
    std::string to_string(Stype t);
    // std::string to_string(Stype t, const int len);
};
