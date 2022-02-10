#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <algorithm>
#include <string_view>

/* 命令の種類 */
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
        constexpr bool is_op();
        constexpr bool is_op_fp();
        constexpr bool is_branch();
        constexpr bool is_branch_fp();
        constexpr bool is_store();
        constexpr bool is_store_fp();
        constexpr bool is_op_imm();
        constexpr bool is_load();
        constexpr bool is_load_fp();
        constexpr bool is_jalr();
        constexpr bool is_jal();
        constexpr bool is_lui();
        constexpr bool is_itof();
        constexpr bool is_ftoi();
        constexpr bool use_mem();
        constexpr bool use_alu();
        constexpr bool use_multicycle_fpu();
        constexpr bool use_pipelined_fpu();
        constexpr bool use_rd_int();
        constexpr bool use_rd_fp();
        constexpr bool use_rs1_int();
        constexpr bool use_rs1_fp();
        constexpr bool use_rs2_int();
        constexpr bool use_rs2_fp();
        constexpr bool branch_conditionally_or_unconditionally();
        constexpr bool is_nop();
        constexpr bool is_exit();
};
inline constexpr unsigned int pipelined_fpu_stage_num = 4;


/* utility functions */
// ターミナルへの出力用
inline constexpr std::string_view head_error = "\033[2D\x1b[34m\x1b[1m\x1b[31mError: \x1b[0m";
inline constexpr std::string_view head_info = "\033[2D\x1b[34m\x1b[32mInfo: \x1b[0m";
inline constexpr std::string_view head_data = "\033[2D\x1b[34mData: \x1b[0m";
inline constexpr std::string_view head_warning = "\033[2D\x1b[33mWarning: \x1b[0m";

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


/* enum class Otype */
// Otypeを文字列に変換
inline std::string string_of_otype(Otype t){
    switch(t){
        case Otype::o_add: return "add";
        case Otype::o_sub: return "sub";
        case Otype::o_sll: return "sll";
        case Otype::o_srl: return "srl";
        case Otype::o_sra: return "sra";
        case Otype::o_and: return "and";
        case Otype::o_fadd: return "fadd";
        case Otype::o_fsub: return "fsub";
        case Otype::o_fmul: return "fmul";
        case Otype::o_fdiv: return "fdiv";
        case Otype::o_fsqrt: return "fsqrt";
        case Otype::o_fcvtif: return "fcvt.i.f";
        case Otype::o_fcvtfi: return "fcvt.f.i";
        case Otype::o_fmvff: return "fmv.f.f";
        case Otype::o_beq: return "beq";
        case Otype::o_blt: return "blt";
        case Otype::o_fbeq: return "fbeq";
        case Otype::o_fblt: return "fblt";
        case Otype::o_sw: return "sw";
        case Otype::o_si: return "si";
        case Otype::o_std: return "std";
        case Otype::o_fsw: return "fsw";
        case Otype::o_addi: return "addi";
        case Otype::o_slli: return "slli";
        case Otype::o_srli: return "srli";
        case Otype::o_srai: return "srai";
        case Otype::o_andi: return "andi";
        case Otype::o_lw: return "lw";
        case Otype::o_lre: return "lre";
        case Otype::o_lrd: return "lrd";
        case Otype::o_ltf: return "ltf";
        case Otype::o_flw: return "flw";
        case Otype::o_jalr: return "jalr";
        case Otype::o_jal: return "jal";
        case Otype::o_lui: return "lui";
        case Otype::o_fmvif: return "fmv.i.f";
        case Otype::o_fmvfi: return "fmv.f.i";
        default: std::exit(EXIT_FAILURE);
    }
}


/* class Bit32 */
inline constexpr Bit32::Bit32(){
    this->i = 0;
}

// intを引数に取るコンストラクタ
inline constexpr Bit32::Bit32(int i){
    this->i = i;
}

// unsigned intを引数に取るコンストラクタ
inline constexpr Bit32::Bit32(unsigned int i){
    this->ui = i;
}

// floatを引数に取るコンストラクタ
inline constexpr Bit32::Bit32(float f){
    this->f = f;
}

// stringに変換して取り出す
inline std::string Bit32::to_string(){
    return std::to_string(this->i);
}
inline std::string Bit32::to_string(Stype t){
    std::string res;
    switch(t){
        case Stype::t_default:
            res = this->to_string();
            break;
        case Stype::t_dec: // 10進数
            res = std::to_string(this->i);
            break;
        case Stype::t_bin: // 2進数
            {
                std::bitset<32> bs(this->i);
                res = bs.to_string();
            }
            break;
        case Stype::t_hex: // 16進数
            {
                std::ostringstream sout;
                sout << std::hex << std::setfill('0') << std::setw(8) << this->i;
                res = "0x" + sout.str();
            }
            break;
        case Stype::t_float: // 浮動小数
            res = std::to_string(this->f);;
            break;
        case Stype::t_op:
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
inline constexpr Operation::Operation(){
    this->type = Otype::o_nop;
    this->rs1 = 0;
    this->rs2 = 0;
    this->imm = 0;
}

// intをもとにするコンストラクタ
inline Operation::Operation(int i){
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
                    this->type = Otype::o_add;
                    return;
                case 1: // sub
                    this->type = Otype::o_sub;
                    return;
                case 2: // sll
                    this->type = Otype::o_sll;
                    return;
                case 3: // srl
                    this->type = Otype::o_srl;
                    return;
                case 4: // sra
                    this->type = Otype::o_sra;
                    return;
                case 5: // and
                    this->type = Otype::o_and;
                    return;
                default: break;
            }
            break;
        case 1: // op_fp
            switch(funct){
                case 0: // fadd
                    this->type = Otype::o_fadd;
                    return;
                case 1: // fsub
                    this->type = Otype::o_fsub;
                    return;
                case 2: // fmul
                    this->type = Otype::o_fmul;
                    return;
                case 3: // fdiv
                    this->type = Otype::o_fdiv;
                    return;
                case 4: // fsqrt
                    this->type = Otype::o_fsqrt;
                    return;
                case 5: // fcvt.i.f
                    this->type = Otype::o_fcvtif;
                    return;
                case 6: // fcvt.f.i
                    this->type = Otype::o_fcvtfi;
                    return;
                case 7: // fmv.f.f
                    this->type = Otype::o_fmvff;
                    return;
                default: break;
            }
            break;
        case 2: // branch
            this->imm = int_of_binary(code.substr(17, 15));
            switch(funct){
                case 0: // beq
                    this->type = Otype::o_beq;
                    return;
                case 1: // blt
                    this->type = Otype::o_blt;
                    return;
                default: break;
            }
            break;
        case 3: // branch_fp
            this->imm = int_of_binary(code.substr(17, 15));
            switch(funct){
                case 2: // fbeq
                    this->type = Otype::o_fbeq;
                    return;
                case 3: // fblt
                    this->type = Otype::o_fblt;
                    return;
                default: break;
            }
            break;
        case 4: // store
            switch(funct){
                case 0: // sw
                    this->type = Otype::o_sw;
                    this->rs1 = rs1;
                    this->imm = int_of_binary(code.substr(17, 15));
                    return;
                case 1: // si
                    this->type = Otype::o_si;
                    this->rs1 = rs1;
                    this->imm = int_of_binary(code.substr(17, 15));
                    return;
                case 2: // std
                    this->type = Otype::o_std;
                    return;
                default: break;
            }
            break;
        case 5: // store_fp
            this->imm = int_of_binary(code.substr(17, 15));
            switch(funct){
                case 0: // fsw
                    this->type = Otype::o_fsw;
                    return;
                default: break;
            }
            break;
        case 6: // op_imm
            this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
            switch(funct){
                case 0: // addi
                    this->type = Otype::o_addi;
                    return;
                case 2: // slli
                    this->type = Otype::o_slli;
                    return;
                case 3: // srli
                    this->type = Otype::o_srli;
                    return;
                case 4: // srai
                    this->type = Otype::o_srai;
                    return;
                case 5: // andi
                    this->type = Otype::o_andi;
                    return;
                default: break;
            }
            break;
        case 7: // load
            switch(funct){
                case 0: // lw
                    this->type = Otype::o_lw;
                    this->rs1 = rs1;
                    this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
                    return;
                case 1: // lre
                    this->type = Otype::o_lre;
                    return;
                case 2: // lrd
                    this->type = Otype::o_lrd;
                    return;
                case 3: // ltf
                    this->type = Otype::o_ltf;
                    return;
                default: break;
            }
        case 8: // load_fp
            this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
            switch(funct){
                case 0: // flw
                    this->type = Otype::o_flw;
                    return;
                default: break;
            }
            break;
        case 9: // jalr
            this->type = Otype::o_jalr;
            return;
        case 10: // jal
            this->type = Otype::o_jal;
            this->imm = int_of_binary(code.substr(4, 13) + code.substr(22, 10));
            return;
        case 11: // lui
            this->imm = int_of_binary("0" + code.substr(7, 10) + code.substr(22, 10));
            switch(funct){
                case 0: // lui
                    this->type = Otype::o_lui;
                    return;
                default: break;
            }
            break;
        case 12: // itof
            switch(funct){
                case 0: // itof
                    this->type = Otype::o_fmvif;
                    return;
                default: break;
            }
            break;
        case 13: // ftoi
            switch(funct){
                case 0: // ftoi
                    this->type = Otype::o_fmvfi;
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
inline constexpr bool Operation::is_op(){
    return this->type == Otype::o_add || this->type == Otype::o_sub || this->type == Otype::o_sll || this->type == Otype::o_srl || this->type == Otype::o_sra || this->type == Otype::o_and;
}
inline constexpr bool Operation::is_op_fp(){
    return this->type == Otype::o_fadd || this->type == Otype::o_fsub || this->type == Otype::o_fmul || this->type == Otype::o_fdiv || this->type == Otype::o_fsqrt || this->type == Otype::o_fcvtif || this->type == Otype::o_fcvtfi || this->type == Otype::o_fmvff;
}
inline constexpr bool Operation::is_branch(){
    return this->type == Otype::o_beq || this->type == Otype::o_blt;
}
inline constexpr bool Operation::is_branch_fp(){
    return this->type == Otype::o_fbeq || this->type == Otype::o_fblt;
}
inline constexpr bool Operation::is_store(){
    return this->type == Otype::o_sw || this->type == Otype::o_si || this->type == Otype::o_std;
}
inline constexpr bool Operation::is_store_fp(){
    return this->type == Otype::o_fsw;
}
inline constexpr bool Operation::is_op_imm(){
    return this->type == Otype::o_addi || this->type == Otype::o_slli || this->type == Otype::o_srli || this->type == Otype::o_srai || this->type == Otype::o_andi;
}
inline constexpr bool Operation::is_load(){
    return this->type == Otype::o_lw || this->type == Otype::o_lre || this->type == Otype::o_lrd || this->type == Otype::o_ltf;
}
inline constexpr bool Operation::is_load_fp(){
    return this->type == Otype::o_flw;
}
inline constexpr bool Operation::is_jalr(){
    return this->type == Otype::o_jalr;
}
inline constexpr bool Operation::is_jal(){
    return this->type == Otype::o_jal;
}
inline constexpr bool Operation::is_lui(){
    return this->type == Otype::o_lui;
}
inline constexpr bool Operation::is_itof(){
    return this->type == Otype::o_fmvif;
}
inline constexpr bool Operation::is_ftoi(){
    return this->type == Otype::o_fmvfi;
}
inline constexpr bool Operation::use_mem(){
    return this->is_load() || this->is_load_fp() || this->is_store() || this->is_store_fp();
}
inline constexpr bool Operation::use_alu(){
    return this->is_op() || this->is_op_imm() || this->is_lui() || this->is_jal() || this->is_jalr() || this->is_itof();
}
inline constexpr bool Operation::use_multicycle_fpu(){
    return this->type == Otype::o_fdiv || this->type == Otype::o_fsqrt || this->type == Otype::o_fcvtif || this->type == Otype::o_fcvtfi || this->type == Otype::o_fmvff || this->type == Otype::o_fmvif;
}
inline constexpr bool Operation::use_pipelined_fpu(){
    return this->type == Otype::o_fadd || this->type == Otype::o_fsub || this->type == Otype::o_fmul;
}
inline constexpr bool Operation::use_rd_int(){
    return this->is_op() || this->is_op_imm() || this->is_lui() || this->is_load() || this->is_jal() || this->is_jalr() || this->is_ftoi();
}
inline constexpr bool Operation::use_rd_fp(){
    return this->is_op_fp() || this->is_load_fp() || this->is_itof();
}
inline constexpr bool Operation::use_rs1_int(){
    return this->is_op() || this->is_op_imm() || this->is_load() || this->is_load_fp() || this->is_store() || this->is_store_fp() || this->is_branch() || this->is_jalr() || this->is_itof();
}
inline constexpr bool Operation::use_rs1_fp(){
    return this->is_op_fp() || this->is_branch_fp() || this->is_ftoi();
}
inline constexpr bool Operation::use_rs2_int(){
    return this->is_op() || this->is_store() || this->is_branch();
}
inline constexpr bool Operation::use_rs2_fp(){
    return this->is_op_fp() || this->is_store_fp() || this->is_branch_fp();
}
inline constexpr bool Operation::branch_conditionally_or_unconditionally(){
    return this->is_branch() || this->is_branch_fp() || this->is_jal() || this->is_jalr();
}
inline constexpr bool Operation::is_nop(){
    return this->type == Otype::o_nop;
}
inline constexpr bool Operation::is_exit(){ // jal x0, 0
    return this->type == Otype::o_jal && this->rd == 0 && this->imm == 0;
}

// 文字列に変換
inline std::string Operation::to_string(){
    switch(this->type){
        case Otype::o_add:
        case Otype::o_sub:
        case Otype::o_sll:
        case Otype::o_srl:
        case Otype::o_sra:
        case Otype::o_and:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", x" + std::to_string(this->rs1) + ", x" + std::to_string(this->rs2);
        case Otype::o_fadd:
        case Otype::o_fsub:
        case Otype::o_fmul:
        case Otype::o_fdiv:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd) + ", f" + std::to_string(this->rs1) + ", f" + std::to_string(this->rs2);
        case Otype::o_fsqrt:
        case Otype::o_fcvtif:
        case Otype::o_fcvtfi:
        case Otype::o_fmvff:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd) + ", f" + std::to_string(this->rs1);
        case Otype::o_beq:
        case Otype::o_blt:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs1) + ", x" + std::to_string(this->rs2) + ", " + std::to_string(this->imm);
        case Otype::o_fbeq:
        case Otype::o_fblt:
            return string_of_otype(this->type) + " f" + std::to_string(this->rs1) + ", f" + std::to_string(this->rs2) + ", " + std::to_string(this->imm);
        case Otype::o_sw:
        case Otype::o_si:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs2) + ", " + std::to_string(this->imm) + "(x" + std::to_string(this->rs1) + ")";
        case Otype::o_std:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs1);
        case Otype::o_fsw:
            return string_of_otype(this->type) + " f" + std::to_string(this->rs2) + ", " + std::to_string(this->imm) + "(x" + std::to_string(this->rs1) + ")";
        case Otype::o_addi:
        case Otype::o_slli:
        case Otype::o_srli:
        case Otype::o_srai:
        case Otype::o_andi:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", x" + std::to_string(this->rs1) + ", " + std::to_string(this->imm);
        case Otype::o_lw:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", " + std::to_string(this->imm) + "(x" + std::to_string(this->rs1) + ")";
        case Otype::o_lre:
        case Otype::o_lrd:
        case Otype::o_ltf:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd);
        case Otype::o_flw:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd) + ", " + std::to_string(this->imm) + "(x" + std::to_string(this->rs1) + ")";
        case Otype::o_jalr:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", x" + std::to_string(this->rs1) + ", " + std::to_string(this->imm);
        case Otype::o_jal:
        case Otype::o_lui:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", " + std::to_string(this->imm);
        case Otype::o_fmvif:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd) + ", x" + std::to_string(this->rs1);
        case Otype::o_fmvfi:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd) + ", f" + std::to_string(this->rs1);
        case Otype::o_nop:
            return "nop";
        default: std::exit(EXIT_FAILURE);
    }
}
