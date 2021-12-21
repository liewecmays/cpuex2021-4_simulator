#pragma once
#include <string>

// 命令の種類
inline constexpr int op_type_num = 37;
enum Otype{
    o_add, o_sub, o_sll, o_srl, o_sra, o_and,
    o_fadd, o_fsub, o_fmul, o_fdiv, o_fsqrt, o_fcvtif, o_fcvtfi, o_fmvff,
    o_beq, o_blt,
    o_fbeq, o_fblt,
    o_sw, o_si, o_std,
    o_fsw,
    o_addi, o_slli, o_srli, o_srai, o_andi,
    o_lw, o_lre, o_lrd, o_ltf,
    o_flw,
    o_jalr,
    o_jal,
    o_lui,
    o_fmvif,
    o_fmvfi
};
std::string string_of_otype(Otype t); // Otypeを文字列に変換

// 命令のクラス
class Operation{
    public:
        Otype type;
        unsigned int rs1;
        unsigned int rs2;
        unsigned int rd;
        int imm;
        Operation();
        Operation(std::string code);
        Operation(int i);
        std::string to_string();
        bool is_op();
        bool is_op_fp();
        bool is_branch();
        bool is_branch_fp();
        bool is_store();
        bool is_store_fp();
        bool is_op_imm();
        bool is_load();
        bool is_load_fp();
        bool is_jalr();
        bool is_jal();
        bool is_lui();
        bool is_itof();
        bool is_ftoi();
        bool use_mem();
        bool use_fpu();
        bool use_rd_int();
        bool use_rd_fp();
        bool use_rs1_int();
        bool use_rs1_fp();
        bool use_rs2_int();
        bool use_rs2_fp();
        bool branch_conditionally_or_unconditionally();
        // bool alu_or_fpu_opcode(); [todo]
};


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


/* インライン展開したいコンストラクタ */
/* class Operation */
// デフォルトコンストラクタではnopを指定
inline Operation::Operation(){
    this->type = Otype::o_add;
    this->rs1 = 0;
    this->rs2 = 0;
    this->imm = 0;
}

/* class Bit32 */
inline Bit32::Bit32(){
    this->i = 0;
}

// intを引数に取るコンストラクタ
inline Bit32::Bit32(int i){
    this->i = i;
}

// unsigned intを引数に取るコンストラクタ
inline Bit32::Bit32(unsigned int i){
    this->ui = i;
}

// floatを引数に取るコンストラクタ
inline Bit32::Bit32(float f){
    this->f = f;
}
