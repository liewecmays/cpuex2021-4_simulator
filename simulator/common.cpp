#include "common.hpp"
#include "util.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <algorithm>

/* class Operation */
// デフォルトコンストラクタではnopを指定
Operation::Operation(){
    this->opcode = 0;
    this->funct = 0;
    this->rs1 = 0;
    this->rs2 = 0;
    this->imm = 0;
}

// stringをパースするコンストラクタ
Operation::Operation(std::string code){
    int opcode, funct, rs1, rs2, rd;    
    opcode = std::stoi(code.substr(0, 4), 0, 2);
    funct = std::stoi(code.substr(4, 3), 0, 2);
    rs1 = std::stoi(code.substr(7, 5), 0, 2);
    rs2 = std::stoi(code.substr(12, 5), 0, 2);
    rd = std::stoi(code.substr(17, 5), 0, 2);
    
    this->opcode = opcode;
    switch(opcode){
        case 0: // op
        case 1: // op_fp
            this->funct = funct;
            this->rs1 = rs1;
            this->rs2 = rs2;
            this->rd = rd;
            this->imm = -1;
            break;
        case 2: // branch
        case 3: // branch_fp
        case 4: // store
        case 5: // store_fp
            this->funct = funct;
            this->rs1 = rs1;
            this->rs2 = rs2;
            this->rd = -1;
            this->imm = int_of_binary(code.substr(17, 15));
            break;
        case 6: // op_imm
        case 7: // load
        case 8: // load_fp
            this->funct = funct;
            this->rs1 = rs1;
            this->rs2 = -1;
            this->rd = rd;
            this->imm = int_of_binary(code.substr(12, 5) + code.substr(22, 10));
            break;
        case 9: // jalr
            this->funct = -1;
            this->rs1 = rs1;
            this->rs2 = -1;
            this->rd = rd;
            this->imm = int_of_binary(code.substr(4, 3) + code.substr(12, 5) + code.substr(22, 10));
            break;
        case 10: // jal
            this->funct = -1;
            this->rs1 = -1;
            this->rs2 = -1;
            this->rd = rd;
            this->imm = int_of_binary(code.substr(4, 13) + code.substr(22, 10));
            break;
        case 11: // long_imm
            this->funct = funct;
            this->rs1 = -1;
            this->rs2 = -1;
            this->rd = rd;
            this->imm = int_of_binary("0" + code.substr(7, 10) + code.substr(22, 10));
            break;
        case 12: // itof
        case 13: // ftoi
            this->funct = funct;
            this->rs1 = rs1;
            this->rs2 = -1;
            this->rd = rd;
            this->imm = -1;
            break;
        default:
            std::cerr << head_error << "could not parse the code" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// 文字列に変換
std::string Operation::to_string(){
    std::string res = "";
    switch(this->opcode){
        case 0: // op
            switch(this->funct){
                case 0: // add
                    res += "add ";
                    break;
                case 1: // sub
                    res += "sub ";
                    break;
                case 2: // sll
                    res += "sll ";
                    break;
                case 3: // srl
                    res += "srl ";
                    break;
                case 4: // sra
                    res += "sra ";
                    break;
                case 5: // and
                    res += "and ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(this->rs1) + ", ");
            res += ("rs2=x" + std::to_string(this->rs2) + ", ");
            res += ("rd=x" + std::to_string(this->rd));
            return res;
        case 1: // op_fp
            switch(this->funct){
                case 0: // fadd
                    res += "fadd ";
                    res += ("rs1=f" + std::to_string(this->rs1) + ", ");
                    res += ("rs2=f" + std::to_string(this->rs2) + ", ");
                    res += ("rd=f" + std::to_string(this->rd));
                    break;
                case 1: // fsub
                    res += "fsub ";
                    res += ("rs1=f" + std::to_string(this->rs1) + ", ");
                    res += ("rs2=f" + std::to_string(this->rs2) + ", ");
                    res += ("rd=f" + std::to_string(this->rd));
                    break;
                case 2: // fmul
                    res += "fmul ";
                    res += ("rs1=f" + std::to_string(this->rs1) + ", ");
                    res += ("rs2=f" + std::to_string(this->rs2) + ", ");
                    res += ("rd=f" + std::to_string(this->rd));
                    break;
                case 3: // fdiv
                    res += "fdiv ";
                    res += ("rs1=f" + std::to_string(this->rs1) + ", ");
                    res += ("rs2=f" + std::to_string(this->rs2) + ", ");
                    res += ("rd=f" + std::to_string(this->rd));
                    break;
                case 4: // fsqrt
                    res += "fsqrt ";
                    res += ("rs1=f" + std::to_string(this->rs1) + ", ");
                    res += ("rd=f" + std::to_string(this->rd));
                    break;
                default: return "";
            }
            return res;
        case 2: // branch
            switch(this->funct){
                case 0: // beq
                    res += "beq ";
                    break;
                case 1: // blt
                    res += "blt ";
                    break;
                case 2: // ble
                    res += "ble ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(this->rs1) + ", ");
            res += ("rs2=x" + std::to_string(this->rs2) + ", ");
            res += ("imm=" + std::to_string(this->imm));
            return res;
        case 3: // branch_fp
            switch(this->funct){
                case 0: // fbeq
                    res += "fbeq ";
                    break;
                case 1: // fblt
                    res += "fblt ";
                    break;
            }
            res += ("rs1=f" + std::to_string(this->rs1) + ", ");
            res += ("rs2=f" + std::to_string(this->rs2) + ", ");
            res += ("imm=" + std::to_string(this->imm));
            return res;
        case 4: // store
            switch(this->funct){
                case 0: // sw
                    res += "sw ";
                    res += ("rs1=x" + std::to_string(this->rs1) + ", ");
                    res += ("rs2=x" + std::to_string(this->rs2) + ", ");
                    res += ("imm=" + std::to_string(this->imm));
                    break;
                case 1: // si
                    res += "si ";
                    res += ("rs1=x" + std::to_string(this->rs1) + ", ");
                    res += ("rs2=x" + std::to_string(this->rs2) + ", ");
                    res += ("imm=" + std::to_string(this->imm));
                    break;
                case 2: // std
                    res += "std ";
                    res += ("rs2=x" + std::to_string(this->rs2));
                    break;
                default: return "";
            }
            
            return res;
        case 5: // store_fp
            switch(this->funct){
                case 0: // fsw
                    res += "fsw ";
                    res += ("rs1=x" + std::to_string(this->rs1) + ", ");
                    res += ("rs2=f" + std::to_string(this->rs2) + ", ");
                    res += ("imm=" + std::to_string(this->imm));
                    break;
                case 1: // fstd
                    res += "fstd ";
                    res += ("rs1=f" + std::to_string(this->rs1));
                    break;
                default: return "";
            }
            return res;
        case 6: // op_imm
            switch(this->funct){
                case 0: // addi
                    res += "addi ";
                    break;
                case 2: // slli
                    res += "slli ";
                    break;
                case 3: // srli
                    res += "srli ";
                    break;
                case 4: // srai
                    res += "srai ";
                    break;
                case 5: // andi
                    res += "andi ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(this->rs1) + ", ");
            res += ("rd=x" + std::to_string(this->rd) + ", ");
            res += ("imm=" + std::to_string(this->imm));
            return res;
        case 7: // load
            switch(this->funct){
                case 0: // lw
                    res += "lw ";
                    res += ("rs1=x" + std::to_string(this->rs1) + ", ");
                    res += ("rd=x" + std::to_string(this->rd) + ", ");
                    res += ("imm=" + std::to_string(this->imm));
                    break;
                case 1: // lre
                    res += "lre ";
                    res += ("rd=x" + std::to_string(this->rd));
                    break;
                case 2: // lrd
                    res += "lrd ";
                    res += ("rd=x" + std::to_string(this->rd));
                    break;
                case 3: // ltf
                    res += "ltf ";
                    res += ("rd=x" + std::to_string(this->rd));
                    break;
                default: return "";
            }
            return res;
        case 8: // load_fp
            switch(this->funct){
                case 0: // lw
                    res += "flw ";
                    res += ("rs1=x" + std::to_string(this->rs1) + ", ");
                    res += ("rd=f" + std::to_string(this->rd) + ", ");
                    res += ("imm=" + std::to_string(this->imm));
                    break;
                case 2: // flrd
                    res += "flrd ";
                    res += ("rd=f" + std::to_string(this->rd));
                    break;
                default: return "";
            }
            return res;
        case 9: // jalr
            res = "jalr ";
            res += ("rs1=x" + std::to_string(this->rs1) + ", ");
            res += ("rd=x" + std::to_string(this->rd) + ", ");
            res += ("imm=" + std::to_string(this->imm));
            return res;
        case 10: // jal
            res = "jal ";
            res += ("rd=x" + std::to_string(this->rd) + ", ");
            res += ("imm=" + std::to_string(this->imm));
            return res;
        case 11: // long_imm
            switch(this->funct){
                case 0: // lui
                    res += "lui ";
                    break;
                case 1: // auipc
                    res += "auipc ";
                    break;
                default: return "";
            }
            res += ("rd=x" + std::to_string(this->rd) + ", ");
            res += ("imm=" + std::to_string(this->imm));
            return res;
        case 12: // itof
            switch(this->funct){
                case 0: // fmv.i.f
                    res += "fmv.i.f ";
                    res += ("rs1=x" + std::to_string(this->rs1) + ", ");
                    res += ("rd=f" + std::to_string(this->rd));
                    break;
                case 5: // fcvt.i.f
                    res += "fcvt.i.f ";
                    res += ("rs1=x" + std::to_string(this->rs1) + ", ");
                    res += ("rd=f" + std::to_string(this->rd));
                    break;
                default: return "";
            }
            return res;
        case 13: // ftoi
            switch(this->funct){
                case 0: // fmv.f.i
                    res += "fmv.f.i ";
                    res += ("rs1=f" + std::to_string(this->rs1) + ", ");
                    res += ("rd=x" + std::to_string(this->rd));
                    break;
                case 6: // fcvt.f.i
                    res += "fcvt.f.i ";
                    res += ("rs1=f" + std::to_string(this->rs1) + ", ");
                    res += ("rd=x" + std::to_string(this->rd));
                    break;
                default: return "";
            }
            return res;
        default: return "";
    }
}


/* class Bit32 */
Bit32::Bit32(){
    this->v = 0;
    this->t = Type::t_int;
}

// intを引数に取るコンストラクタ
Bit32::Bit32(int i){
    this->v = i;
    this->t = Type::t_int;
}

// intを引数に取るが、型を指定できるコンストラクタ
Bit32::Bit32(int i, Type t){
    this->v = i;
    this->t = t;
}

// floatを引数に取るコンストラクタ
Bit32::Bit32(float f){
    Int_float u;
    u.f = f;
    this->v = u.i;
    this->t = Type::t_float;
}

// intを取り出す
int Bit32::to_int(){
    return this->v;
}

// floatに変換して取り出す
float Bit32::to_float(){
    Int_float u;
    u.i = this->v;
    return u.f;
}

// stringに変換して取り出す
std::string Bit32::to_string(){
    switch(this->t){
        case Type::t_int:
            return std::to_string((*this).to_int());
        case Type::t_float:
            return std::to_string((*this).to_float());
        default: std::exit(EXIT_FAILURE);
    }
}

std::string Bit32::to_string(Stype t){
    std::string res;
    switch(t){
        case Stype::t_default:
            res = this->to_string();
            break;
        case Stype::t_dec: // 10進数
            res = std::to_string(this->v);
            break;
        case Stype::t_bin: // 2進数
            {
                std::bitset<32> bs(this->v);
                res = "0b" + bs.to_string();
            }
            break;
        case Stype::t_hex: // 16進数
            {
                std::ostringstream sout;
                sout << std::hex << std::setfill('0') << std::setw(8) << this->v;
                res = "0x" + sout.str();
            }
            break;
        case Stype::t_float: // 浮動小数
            res = std::to_string(this->to_float());;
            break;
        case Stype::t_op:
            {
                std::bitset<32> bs(this->v);
                res = Operation(bs.to_string()).to_string();
            }
            break;
        default: std::exit(EXIT_FAILURE);
    }

    return res;
}

std::string Bit32::to_string(Stype t, const int len){
    std::string res;
    switch(t){
        case Stype::t_dec: // 10進数
            {
                std::ostringstream sout;
                sout << std::setfill('0') << std::setw(len) << this->v;
                res = sout.str();
            }
            break;
        case Stype::t_bin: // 2進数
            {
                int n = this->v;
                while (n > 0) {
                    res.push_back('0' + (n & 1));
                    n >>= 1;
                }
                std::reverse(res.begin(), res.end());
                res = "0b" + res;
            }
            break;
        case Stype::t_hex: // 16進数
            {
                std::ostringstream sout;
                sout << std::hex << std::setfill('0') << std::setw(len) << this->v;
                res = "0x" + sout.str();
            }
            break;
        case Stype::t_float: // 浮動小数
            {
                std::ostringstream sout;
                sout << std::setprecision(len) << this->to_float();
                res = sout.str();
            }
            break;
        default:
            std::cerr << head_error << "do not designate length with t_default/t_op" << std::endl;
            std::exit(EXIT_FAILURE);
    }

    return res;
}