#pragma once
#include <string>

// 命令の種類
inline constexpr int op_type_num = 38;
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
    o_fmvfi,
    o_nop // コアの実装では'1などに対応(内部的にしか使わない)
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
        bool use_multicycle_fpu();
        bool use_pipelined_fpu();
        bool use_rd_int();
        bool use_rd_fp();
        bool use_rs1_int();
        bool use_rs1_fp();
        bool use_rs2_int();
        bool use_rs2_fp();
        bool branch_conditionally_or_unconditionally();
        bool is_nop();
        // bool alu_or_fpu_opcode(); [todo]
};
inline constexpr unsigned int pipelined_fpu_stage_num = 4;
extern Operation nop;


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

// レジスタ
class Reg{
    private:
        Bit32 val[32];
    public:
        Bit32 read_32(unsigned int);
        int read_int(unsigned int);
        float read_float(unsigned int);
        void write_32(unsigned int, Bit32);
        void write_int(unsigned int, int);
        void write_float(unsigned int, float);
};

/* インライン展開したいコンストラクタ */
/* class Operation */
// デフォルトコンストラクタではnopを指定
inline Operation::Operation(){
    this->type = Otype::o_nop;
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
