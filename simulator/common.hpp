#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <algorithm>
#include <atomic>
#include <string_view>

/* シミュレータの状態管理 */
inline constexpr int sim_state_continue = -1;
inline constexpr int sim_state_end = -2;


/* ターミナルへの出力用 */
inline constexpr std::string_view head_error = "\033[2D\x1b[34m\x1b[1m\x1b[31mError: \x1b[0m";
inline constexpr std::string_view head_info = "\033[2D\x1b[34m\x1b[32mInfo: \x1b[0m";
inline constexpr std::string_view head_data = "\033[2D\x1b[34mData: \x1b[0m";
inline constexpr std::string_view head_warning = "\033[2D\x1b[33mWarning: \x1b[0m";


/* 命令の種類 */
inline constexpr int op_type_num = 39;
enum Otype{
    o_add, o_sub, o_sll, o_srl, o_sra, o_and,
    o_fabs, o_fneg, o_fdiv, o_fsqrt, o_fcvtif, o_fcvtfi, o_fmvff,
    o_fadd, o_fsub, o_fmul,
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

/* 文字列変換の際に指定対象となる型 */
enum class Stype{
    t_default, t_dec, t_bin, t_hex, t_float, t_op
};

/* 内部表現を考慮したfloat */
struct Float{
    unsigned int m : 23;
    unsigned int e : 8;
    unsigned int s : 1;
};

/* 32ビット列を扱うための共用体 */
union Bit32{
    int i;
    unsigned int ui;
    float f;
    Float F;
    constexpr Bit32();
    constexpr Bit32(int i);
    constexpr Bit32(unsigned int ui);
    constexpr Bit32(float f);
    std::string to_string();
    std::string to_string(Stype t);
};

/* 命令のクラス */
class Operation{
    public:
        Otype type;
        unsigned int rs1;
        unsigned int rs2;
        unsigned int rd;
        int imm;
        constexpr Operation();
        Operation(std::string code);
        Operation(int i);
        std::string to_string();
        constexpr bool is_op() const;
        constexpr bool is_op_fp() const;
        constexpr bool is_branch() const;
        constexpr bool is_branch_fp() const;
        constexpr bool is_conditional() const;
        constexpr bool is_unconditional() const;
        constexpr bool is_store() const;
        constexpr bool is_store_fp() const;
        constexpr bool is_op_imm() const;
        constexpr bool is_load() const;
        constexpr bool is_load_fp() const;
        constexpr bool is_lw_flw_sw_fsw() const;
        constexpr bool is_jalr() const;
        constexpr bool is_jal() const;
        constexpr bool is_lui() const;
        constexpr bool is_itof() const;
        constexpr bool is_ftoi() const;
        constexpr bool use_mem() const;
        constexpr bool use_alu() const;
        constexpr bool use_multicycle_fpu() const;
        constexpr bool use_pipelined_fpu() const;
        constexpr bool use_rd_int() const;
        constexpr bool use_rd_fp() const;
        constexpr bool use_rs1_int() const;
        constexpr bool use_rs1_fp() const;
        constexpr bool use_rs2_int() const;
        constexpr bool use_rs2_fp() const;
        constexpr bool branch_conditionally_or_unconditionally() const;
        constexpr bool is_zero_latency_mfp() const;
        constexpr bool is_nonzero_latency_mfp() const;
        constexpr bool is_nop() const;
        constexpr bool is_exit() const;
};

/* プロトタイプ宣言 */
int int_of_binary(std::string);
std::string binary_of_int(int);
std::string binary_of_float(float);
std::string data_of_int(int);
std::string data_of_float(float);
std::string data_of_binary(std::string);
Bit32 bit32_of_data(std::string);
constexpr unsigned int take_bit(unsigned int, int);
constexpr unsigned int take_bits(unsigned int, int, int);
constexpr unsigned long long take_bits(unsigned long long, int, int);
constexpr unsigned int isset_bit(unsigned int, unsigned int);

using enum Otype;
using enum Stype;

/* enum class Otype */
// Otypeを文字列に変換
inline std::string string_of_otype(Otype t){
    switch(t){
        case o_add: return "add";
        case o_sub: return "sub";
        case o_sll: return "sll";
        case o_srl: return "srl";
        case o_sra: return "sra";
        case o_and: return "and";
        case o_fabs: return "fabs";
        case o_fneg: return "fneg";
        case o_fdiv: return "fdiv";
        case o_fsqrt: return "fsqrt";
        case o_fcvtif: return "fcvt.i.f";
        case o_fcvtfi: return "fcvt.f.i";
        case o_fmvff: return "fmv.f.f";
        case o_fadd: return "fadd";
        case o_fsub: return "fsub";
        case o_fmul: return "fmul";
        case o_beq: return "beq";
        case o_blt: return "blt";
        case o_fbeq: return "fbeq";
        case o_fblt: return "fblt";
        case o_sw: return "sw";
        case o_si: return "si";
        case o_std: return "std";
        case o_fsw: return "fsw";
        case o_addi: return "addi";
        case o_slli: return "slli";
        case o_srli: return "srli";
        case o_srai: return "srai";
        case o_andi: return "andi";
        case o_lw: return "lw";
        case o_lre: return "lre";
        case o_lrd: return "lrd";
        case o_ltf: return "ltf";
        case o_flw: return "flw";
        case o_jalr: return "jalr";
        case o_jal: return "jal";
        case o_lui: return "lui";
        case o_fmvif: return "fmv.i.f";
        case o_fmvfi: return "fmv.f.i";
        default: std::exit(EXIT_FAILURE);
    }
}


/* class Bit32 */
inline constexpr Bit32::Bit32() noexcept {
    this->i = 0;
}

// intを引数に取るコンストラクタ
inline constexpr Bit32::Bit32(int i) noexcept {
    this->i = i;
}

// unsigned intを引数に取るコンストラクタ
inline constexpr Bit32::Bit32(unsigned int i) noexcept {
    this->ui = i;
}

// floatを引数に取るコンストラクタ
inline constexpr Bit32::Bit32(float f) noexcept {
    this->f = f;
}

// stringに変換して取り出す
inline std::string Bit32::to_string(){
    return std::to_string(this->i);
}
inline std::string Bit32::to_string(Stype t){
    std::string res;
    switch(t){
        case t_default:
            res = this->to_string();
            break;
        case t_dec: // 10進数
            res = std::to_string(this->i);
            break;
        case t_bin: // 2進数
            {
                std::bitset<32> bs(this->i);
                res = bs.to_string();
            }
            break;
        case t_hex: // 16進数
            {
                std::ostringstream sout;
                sout << std::hex << std::setfill('0') << std::setw(8) << this->i;
                res = "0x" + sout.str();
            }
            break;
        case t_float: // 浮動小数
            res = std::to_string(this->f);;
            break;
        case t_op:
            {
                std::bitset<32> bs(this->i);
                res = Operation(bs.to_string()).to_string();
            }
            break;
        default: std::exit(EXIT_FAILURE);
    }

    return res;
}


/* class Operation */
// デフォルトコンストラクタではnopを指定
inline constexpr Operation::Operation() noexcept {
    this->type = o_nop;
    this->rs1 = 0;
    this->rs2 = 0;
    this->imm = 0;
}

// intをもとにするコンストラクタ
inline Operation::Operation(int i) noexcept {
    std::stringstream code;
    code << std::bitset<32>(i);
    (*this) = Operation(code.str());
}

// stringをパースするコンストラクタ
inline Operation::Operation(std::string code){
    unsigned int opcode = std::stoi(code.substr(0, 4), 0, 2);
    unsigned int funct = std::stoi(code.substr(4, 3), 0, 2);
    this->rs1 = std::stoi(code.substr(7, 5), 0, 2);
    this->rs2 = std::stoi(code.substr(12, 5), 0, 2);
    this->rd = std::stoi(code.substr(17, 5), 0, 2);
    this->imm = 0;

    switch(opcode){
        case 0: // op
            switch(funct){
                case 0: // add
                    this->type = o_add;
                    return;
                case 1: // sub
                    this->type = o_sub;
                    return;
                case 2: // sll
                    this->type = o_sll;
                    return;
                case 3: // srl
                    this->type = o_srl;
                    return;
                case 4: // sra
                    this->type = o_sra;
                    return;
                case 5: // and
                    this->type = o_and;
                    return;
                default: break;
            }
            break;
        case 1: // op_mfp
            switch(funct){
                case 0: // fabs
                    this->type = o_fabs;
                    return;
                case 1: // fneg
                    this->type = o_fneg;
                    return;
                case 3: // fdiv
                    this->type = o_fdiv;
                    return;
                case 4: // fsqrt
                    this->type = o_fsqrt;
                    return;
                case 5: // fcvt.i.f
                    this->type = o_fcvtif;
                    return;
                case 6: // fcvt.f.i
                    this->type = o_fcvtfi;
                    return;
                case 7: // fmv.f.f
                    this->type = o_fmvff;
                    return;
                default: break;
            }
            break;
        case 2: // op_pfp
            switch(funct){
                case 0: // fadd
                    this->type = o_fadd;
                    return;
                case 1: // fsub
                    this->type = o_fsub;
                    return;
                case 2: // fmul
                    this->type = o_fmul;
                    return;
                default: break;
            }
            break;
        case 3: // branch
            this->imm = int_of_binary(code.substr(17, 15));
            switch(funct){
                case 0: // beq
                    this->type = o_beq;
                    return;
                case 1: // blt
                    this->type = o_blt;
                    return;
                default: break;
            }
            break;
        case 4: // branch_fp
            this->imm = int_of_binary(code.substr(17, 15));
            switch(funct){
                case 2: // fbeq
                    this->type = o_fbeq;
                    return;
                case 3: // fblt
                    this->type = o_fblt;
                    return;
                default: break;
            }
            break;
        case 5: // store
            switch(funct){
                case 0: // sw
                    this->type = o_sw;
                    this->rs1 = rs1;
                    this->imm = int_of_binary(code.substr(17, 15));
                    return;
                case 1: // si
                    this->type = o_si;
                    this->rs1 = rs1;
                    this->imm = int_of_binary(code.substr(17, 15));
                    return;
                case 2: // std
                    this->type = o_std;
                    return;
                default: break;
            }
            break;
        case 6: // store_fp
            this->imm = int_of_binary(code.substr(17, 15));
            switch(funct){
                case 0: // fsw
                    this->type = o_fsw;
                    return;
                default: break;
            }
            break;
        case 7: // op_imm
            this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
            switch(funct){
                case 0: // addi
                    this->type = o_addi;
                    return;
                case 2: // slli
                    this->type = o_slli;
                    return;
                case 3: // srli
                    this->type = o_srli;
                    return;
                case 4: // srai
                    this->type = o_srai;
                    return;
                case 5: // andi
                    this->type = o_andi;
                    return;
                default: break;
            }
            break;
        case 8: // load
            switch(funct){
                case 0: // lw
                    this->type = o_lw;
                    this->rs1 = rs1;
                    this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
                    return;
                case 1: // lre
                    this->type = o_lre;
                    return;
                case 2: // lrd
                    this->type = o_lrd;
                    return;
                case 3: // ltf
                    this->type = o_ltf;
                    return;
                default: break;
            }
            break;
        case 9: // load_fp
            this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
            switch(funct){
                case 0: // flw
                    this->type = o_flw;
                    return;
                default: break;
            }
            break;
        case 10: // jalr
            switch(funct){
                case 0: // jalr
                    this->type = o_jalr;
                    return;
                default: break;
            }
            break;
        case 11: // jal
            this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
            switch(funct){
                case 0: // jal
                    this->type = o_jal;
                    return;
                default: break;
            }
            break;
        case 12: // lui
            this->imm = int_of_binary("0" + code.substr(7, 10) + code.substr(22, 10));
            switch(funct){
                case 0: // lui
                    this->type = o_lui;
                    return;
                default: break;
            }
            break;
        case 13: // itof
            switch(funct){
                case 0: // itof
                    this->type = o_fmvif;
                    return;
                default: break;
            }
            break;
        case 14: // ftoi
            switch(funct){
                case 0: // ftoi
                    this->type = o_fmvfi;
                    return;
                default: break;
            }
            break;
        default:
            std::cerr << head_error << "could not parse the code" << std::endl;
            std::exit(EXIT_FAILURE);
    }

    std::cerr << head_error << "could not parse the code" << std::endl;
    std::exit(EXIT_FAILURE);
}

// opの属性に関する判定
inline constexpr bool Operation::is_op() const noexcept {
    return this->type == o_add || this->type == o_sub || this->type == o_sll || this->type == o_srl || this->type == o_sra || this->type == o_and;
}
inline constexpr bool Operation::is_op_fp() const noexcept {
    return this->type == o_fadd || this->type == o_fsub || this->type == o_fabs || this->type == o_fneg || this->type == o_fmul || this->type == o_fdiv || this->type == o_fsqrt || this->type == o_fcvtif || this->type == o_fcvtfi || this->type == o_fmvff;
}
inline constexpr bool Operation::is_branch() const noexcept {
    return this->type == o_beq || this->type == o_blt;
}
inline constexpr bool Operation::is_branch_fp() const noexcept {
    return this->type == o_fbeq || this->type == o_fblt;
}
inline constexpr bool Operation::is_conditional() const noexcept {
    return this->is_branch() || this->is_branch_fp();
}
inline constexpr bool Operation::is_unconditional() const noexcept {
    return this->type == o_jalr || this->type == o_jal;
}
inline constexpr bool Operation::is_store() const noexcept {
    return this->type == o_sw || this->type == o_si || this->type == o_std;
}
inline constexpr bool Operation::is_store_fp() const noexcept {
    return this->type == o_fsw;
}
inline constexpr bool Operation::is_op_imm() const noexcept {
    return this->type == o_addi || this->type == o_slli || this->type == o_srli || this->type == o_srai || this->type == o_andi;
}
inline constexpr bool Operation::is_load() const noexcept {
    return this->type == o_lw || this->type == o_lre || this->type == o_lrd || this->type == o_ltf;
}
inline constexpr bool Operation::is_load_fp() const noexcept {
    return this->type == o_flw;
}
inline constexpr bool Operation::is_lw_flw_sw_fsw() const noexcept {
    return this->type == o_lw || this->type == o_flw || this->type == o_sw || this->type == o_fsw;
}
inline constexpr bool Operation::is_jalr() const noexcept {
    return this->type == o_jalr;
}
inline constexpr bool Operation::is_jal() const noexcept {
    return this->type == o_jal;
}
inline constexpr bool Operation::is_lui() const noexcept {
    return this->type == o_lui;
}
inline constexpr bool Operation::is_itof() const noexcept {
    return this->type == o_fmvif;
}
inline constexpr bool Operation::is_ftoi() const noexcept {
    return this->type == o_fmvfi;
}
inline constexpr bool Operation::use_mem() const noexcept {
    return this->is_load() || this->is_load_fp() || this->is_store() || this->is_store_fp();
}
inline constexpr bool Operation::use_alu() const noexcept {
    return this->is_op() || this->is_op_imm() || this->is_lui() || this->is_jal() || this->is_jalr() || this->is_itof();
}
inline constexpr bool Operation::use_multicycle_fpu() const noexcept {
    return this->type == o_fabs || this->type == o_fneg || this->type == o_fdiv || this->type == o_fsqrt || this->type == o_fcvtif || this->type == o_fcvtfi || this->type == o_fmvff || this->type == o_fmvif;
}
inline constexpr bool Operation::use_pipelined_fpu() const noexcept {
    return this->type == o_fadd || this->type == o_fsub || this->type == o_fmul;
}
inline constexpr bool Operation::use_rd_int() const noexcept {
    return this->is_op() || this->is_op_imm() || this->is_lui() || this->is_load() || this->is_jal() || this->is_jalr() || this->is_ftoi();
}
inline constexpr bool Operation::use_rd_fp() const noexcept {
    return this->is_op_fp() || this->is_load_fp() || this->is_itof();
}
inline constexpr bool Operation::use_rs1_int() const noexcept {
    return this->is_op() || this->is_op_imm() || this->is_load() || this->is_load_fp() || this->is_store() || this->is_store_fp() || this->is_branch() || this->is_jalr() || this->is_itof();
}
inline constexpr bool Operation::use_rs1_fp() const noexcept {
    return this->is_op_fp() || this->is_branch_fp() || this->is_ftoi();
}
inline constexpr bool Operation::use_rs2_int() const noexcept {
    return this->is_op() || this->is_store() || this->is_branch();
}
inline constexpr bool Operation::use_rs2_fp() const noexcept {
    return this->is_op_fp() || this->is_store_fp() || this->is_branch_fp();
}
inline constexpr bool Operation::branch_conditionally_or_unconditionally() const noexcept {
    return this->is_branch() || this->is_branch_fp() || this->is_jal() || this->is_jalr();
}
inline constexpr bool Operation::is_zero_latency_mfp() const noexcept {
    return this->type == o_fabs || this->type == o_fneg || this->type == o_fmvif || this->type == o_fmvff;
}
inline constexpr bool Operation::is_nonzero_latency_mfp() const noexcept {
    return this->type == o_fdiv || this->type == o_fsqrt || this->type == o_fcvtif || this->type == o_fcvtfi;
}
inline constexpr bool Operation::is_nop() const noexcept {
    return this->type == o_nop;
}
inline constexpr bool Operation::is_exit() const noexcept { // jal x0, 0
    return this->type == o_jal && this->rd == 0 && this->imm == 0;
}

// 文字列に変換
inline std::string Operation::to_string(){
    switch(this->type){
        case o_add:
        case o_sub:
        case o_sll:
        case o_srl:
        case o_sra:
        case o_and:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", x" + std::to_string(this->rs1) + ", x" + std::to_string(this->rs2);
        case o_fadd:
        case o_fsub:
        case o_fmul:
        case o_fdiv:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd) + ", f" + std::to_string(this->rs1) + ", f" + std::to_string(this->rs2);
        case o_fabs:
        case o_fneg:
        case o_fsqrt:
        case o_fcvtif:
        case o_fcvtfi:
        case o_fmvff:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd) + ", f" + std::to_string(this->rs1);
        case o_beq:
        case o_blt:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs1) + ", x" + std::to_string(this->rs2) + ", " + std::to_string(this->imm);
        case o_fbeq:
        case o_fblt:
            return string_of_otype(this->type) + " f" + std::to_string(this->rs1) + ", f" + std::to_string(this->rs2) + ", " + std::to_string(this->imm);
        case o_sw:
        case o_si:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs2) + ", " + std::to_string(this->imm) + "(x" + std::to_string(this->rs1) + ")";
        case o_std:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs1);
        case o_fsw:
            return string_of_otype(this->type) + " f" + std::to_string(this->rs2) + ", " + std::to_string(this->imm) + "(x" + std::to_string(this->rs1) + ")";
        case o_addi:
        case o_slli:
        case o_srli:
        case o_srai:
        case o_andi:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", x" + std::to_string(this->rs1) + ", " + std::to_string(this->imm);
        case o_lw:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", " + std::to_string(this->imm) + "(x" + std::to_string(this->rs1) + ")";
        case o_lre:
        case o_lrd:
        case o_ltf:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd);
        case o_flw:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd) + ", " + std::to_string(this->imm) + "(x" + std::to_string(this->rs1) + ")";
        case o_jalr:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", x" + std::to_string(this->rs1) + ", " + std::to_string(this->imm);
        case o_jal:
        case o_lui:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", " + std::to_string(this->imm);
        case o_fmvif:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd) + ", x" + std::to_string(this->rs1);
        case o_fmvfi:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", f" + std::to_string(this->rs1);
        case o_nop:
            return "nop";
        default: std::exit(EXIT_FAILURE);
    }
}


/* utility functions */
// 2進数を表す文字列から整数に変換
inline int int_of_binary(std::string s){
    if(s == ""){
        std::cerr << head_error << "invalid input to 'int_of_binary'" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    int length = s.size(), res = 0, d;
    if(s[0] == '0'){ // 正数
        d = 1 << (length - 2);
        for(auto c : s.substr(1)){
            res += c == '1' ? d : 0;
            d = d >> 1;
        }
    }else{ // 負数
        d = 1 << 30;
        for(int i = 30; i>=0; i--){
            if(i >= length){ // 符号拡張
                res += d;
            }else{
                res += s[length - 1 - i] == '1' ? d : 0;
            }
            d = d >> 1;
        }
        res = res - (1 << 31);
    }
    return res;
}

// 10進数を2進数の文字列へと変換
inline std::string binary_of_int(int i){
    std::bitset<32> bs(i);
    return bs.to_string();
}

// 浮動小数点数を2進数の文字列へと変換
inline std::string binary_of_float(float f){
    std::bitset<32> bs(Bit32(f).i);
    return bs.to_string();
}

// 整数を送信データへと変換
inline std::string data_of_int(int i){
    return binary_of_int(i);
}

// 浮動小数点数を送信データへと変換
inline std::string data_of_float(float f){
    return binary_of_float(f);
}

// 2進数の文字列を送信データへと変換
inline std::string data_of_binary(std::string s){
    if(s.size() <= 32){
        while(s.size() < 32){
            s = "0" + s;
        }
    }else{
        std::cerr << head_error << "invalid input to 'data_of_binary'" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return s;
}

// 送信データをBit32へと変換
inline Bit32 bit32_of_data(std::string data){
    if(data.size() < 32){
        data = "0" + data; // 8ビットのデータなので、先頭に0を追加して変換
    }
    return Bit32(int_of_binary(data));
    // if(data[0] == 'i'){ // int
    //     return Bit32(int_of_binary(data.substr(1,32)));
    // }else if(data[0] == 'f'){ // float
    //     return Bit32(int_of_binary(data.substr(1,32)));
    // }else{
    //     std::cerr << head_error << "invalid input to 'bit32_of_data'" << std::endl;
    //     std::exit(EXIT_FAILURE);
    // }
}

// x[n]
constexpr inline unsigned int take_bit(unsigned int x, int n) noexcept {
    return n >= 0 ? (x >> n) & 1 : 0;
}

// x[to:from]
constexpr inline unsigned int take_bits(unsigned int x, int from, int to) noexcept {
    if(to >= 0){
        if(from >= 0){
            return (x >> from) & ((1 << (to - from + 1)) - 1);
        }else{
            return (x & ((1 << (to + 1)) - 1)) << (-from); // x[to:0] << (-from)
        }
    }else{
        return 0;
    }
}

// x[to:from]
constexpr inline unsigned long long take_bits(unsigned long long x, int from, int to) noexcept {
    if(to >= 0){
        if(from >= 0){
            return (x >> from) & ((1 << (to - from + 1)) - 1);
        }else{
            return (x & ((1 << (to + 1)) - 1)) << (-from); // x[to:0] << (-from)
        }
    }else{
        return 0;
    }
}

// x[n] == 1
constexpr inline unsigned int isset_bit(unsigned int x, unsigned int n) noexcept {
    return ((x >> n) & 1) == 1;
}


/* スレッドの管理用フラグ */
class Cancel_flag{
    std::atomic<bool> signaled_{ false };
    public:
        void signal(){signaled_ = true;}
        bool operator!() {return !signaled_;}
};
