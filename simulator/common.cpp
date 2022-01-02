#include <common.hpp>
#include <util.hpp>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <algorithm>

using ::Otype;

/* class Operation */
// stringをパースするコンストラクタ
Operation::Operation(std::string code){
    if(code == "nop"){
        this->type = o_nop;
        return;
    }

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
        case 1: // op_fp
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
        case 2: // branch
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
        case 3: // branch_fp
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
        case 4: // store
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
        case 5: // store_fp
            this->imm = int_of_binary(code.substr(17, 15));
            switch(funct){
                case 0: // fsw
                    this->type = o_fsw;
                    return;
                default: break;
            }
            break;
        case 6: // op_imm
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
        case 7: // load
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
        case 8: // load_fp
            this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
            switch(funct){
                case 0: // flw
                    this->type = o_flw;
                    return;
                default: break;
            }
            break;
        case 9: // jalr
            this->type = o_jalr;
            return;
        case 10: // jal
            this->type = o_jal;
            this->imm = int_of_binary(code.substr(4, 13) + code.substr(22, 10));
            return;
        case 11: // lui
            this->imm = int_of_binary("0" + code.substr(7, 10) + code.substr(22, 10));
            switch(funct){
                case 0: // lui
                    this->type = o_lui;
                    return;
                default: break;
            }
            break;
        case 12: // itof
            switch(funct){
                case 0: // itof
                    this->type = o_fmvif;
                    return;
                default: break;
            }
            break;
        case 13: // ftoi
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

// nopの定義
Operation nop = Operation("nop");

// intをもとにするコンストラクタ
Operation::Operation(int i){
    std::stringstream code;
    code << std::bitset<32>(i);
    (*this) = Operation(code.str());
}

// opの属性に関する判定
bool Operation::is_op(){
    return this->type == o_add || this->type == o_sub || this->type == o_sll || this->type == o_srl || this->type == o_sra || this->type == o_and;
}
bool Operation::is_op_fp(){
    return this->type == o_fadd || this->type == o_fsub || this->type == o_fmul || this->type == o_fdiv || this->type == o_fsqrt || this->type == o_fcvtif || this->type == o_fcvtfi || this->type == o_fmvff;
}
bool Operation::is_branch(){
    return this->type == o_beq || this->type == o_blt;
}
bool Operation::is_branch_fp(){
    return this->type == o_fbeq || this->type == o_fblt;
}
bool Operation::is_store(){
    return this->type == o_sw || this->type == o_si || this->type == o_std;
}
bool Operation::is_store_fp(){
    return this->type == o_fsw;
}
bool Operation::is_op_imm(){
    return this->type == o_addi || this->type == o_slli || this->type == o_srli || this->type == o_srai || this->type == o_andi;
}
bool Operation::is_load(){
    return this->type == o_lw || this->type == o_lre || this->type == o_lrd || this->type == o_ltf;
}
bool Operation::is_load_fp(){
    return this->type == o_flw;
}
bool Operation::is_jalr(){
    return this->type == o_jalr;
}
bool Operation::is_jal(){
    return this->type == o_jal;
}
bool Operation::is_lui(){
    return this->type == o_lui;
}
bool Operation::is_itof(){
    return this->type == o_fmvif;
}
bool Operation::is_ftoi(){
    return this->type == o_fmvfi;
}
bool Operation::use_mem(){
    return this->is_load() || this->is_load_fp() || this->is_store() || this->is_store_fp();
}
bool Operation::use_multicycle_fpu(){
    return this->type == o_fdiv || this->type == o_fsqrt || this->type == o_fcvtif || this->type == o_fcvtfi || this->type == o_fmvff || this->type == o_fmvif;
}
bool Operation::use_pipelined_fpu(){
    return this->type == o_fadd || this->type == o_fsub || this->type == o_fmul;
}
bool Operation::use_rd_int(){
    return this->is_op() || this->is_op_imm() || this->is_lui() || this->is_load() || this->is_jal() || this->is_jalr() || this->is_ftoi();
}
bool Operation::use_rd_fp(){
    return this->is_op_fp() || this->is_load_fp() || this->is_itof();
}
bool Operation::use_rs1_int(){
    return this->is_op() || this->is_op_imm() || this->is_load() || this->is_load_fp() || this->is_store() || this->is_store_fp() || this->is_branch() || this->is_jalr() || this->is_itof();
}
bool Operation::use_rs1_fp(){
    return this->is_op_fp() || this->is_branch_fp() || this->is_ftoi();
}
bool Operation::use_rs2_int(){
    return this->is_op() || this->is_store() || this->is_branch();
}
bool Operation::use_rs2_fp(){
    return this->is_op_fp() || this->is_store_fp() || this->is_branch_fp();
}
bool Operation::branch_conditionally_or_unconditionally(){
    return this->is_branch() || this->is_branch_fp() || this->is_jal() || this->is_jalr();
}
bool Operation::is_nop(){
    return this->type == o_nop;
}

// 文字列に変換
std::string Operation::to_string(){
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

// Otypeを文字列に変換
std::string string_of_otype(Otype t){
    switch(t){
        case o_add: return "add";
        case o_sub: return "sub";
        case o_sll: return "sll";
        case o_srl: return "srl";
        case o_sra: return "sra";
        case o_and: return "and";
        case o_fadd: return "fadd";
        case o_fsub: return "fsub";
        case o_fmul: return "fmul";
        case o_fdiv: return "fdiv";
        case o_fsqrt: return "fsqrt";
        case o_fcvtif: return "fcvt.i.f";
        case o_fcvtfi: return "fcvt.f.i";
        case o_fmvff: return "fmv.f.f";
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
// stringに変換して取り出す
std::string Bit32::to_string(){
    return std::to_string(this->i);
}

std::string Bit32::to_string(Stype t){
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


/* class Reg */
// read
Bit32 Reg::read_32(unsigned int i){
    return i == 0 ? Bit32(0) : this->val[i];
}
int Reg::read_int(unsigned int i){
    return i == 0 ? 0 : this->val[i].i;
}
float Reg::read_float(unsigned int i){
    return i == 0 ? 0.0f : this->val[i].f;
}

// write
void Reg::write_32(unsigned int i, Bit32 v){
    if(i != 0) this->val[i] = v;
}
void Reg::write_int(unsigned int i, int v){
    if(i != 0) this->val[i] = Bit32(v);
}
void Reg::write_float(unsigned int i, float v){
    if(i != 0) this->val[i] = Bit32(v);
}
