#include <common.hpp>
#include <util.hpp>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <algorithm>

/* class Operation */
// stringをパースするコンストラクタ
Operation::Operation(std::string code){
    unsigned int opcode, funct, rs1, rs2, rd;    
    opcode = std::stoi(code.substr(0, 4), 0, 2);
    funct = std::stoi(code.substr(4, 3), 0, 2);
    rs1 = std::stoi(code.substr(7, 5), 0, 2);
    rs2 = std::stoi(code.substr(12, 5), 0, 2);
    rd = std::stoi(code.substr(17, 5), 0, 2);

    switch(opcode){
        case 0: // op
            this->rs1 = rs1;
            this->rs2 = rs2;
            this->rd = rd;
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
            this->rs1 = rs1;
            this->rd = rd;
            switch(funct){
                case 0: // fadd
                    this->type = Otype::o_fadd;
                    this->rs2 = rs2;
                    return;
                case 1: // fsub
                    this->type = Otype::o_fsub;
                    this->rs2 = rs2;
                    return;
                case 2: // fmul
                    this->type = Otype::o_fmul;
                    this->rs2 = rs2;
                    return;
                case 3: // fdiv
                    this->type = Otype::o_fdiv;
                    this->rs2 = rs2;
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
                default: break;
            }
            break;
        case 2: // branch
            this->rs1 = rs1;
            this->rs2 = rs2;
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
            this->rs1 = rs1;
            this->rs2 = rs2;
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
            this->rs2 = rs2;
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
            this->rs1 = rs1;
            this->rs2 = rs2;
            this->imm = int_of_binary(code.substr(17, 15));
            switch(funct){
                case 0: // fsw
                    this->type = Otype::o_fsw;
                    return;
                default: break;
            }
            break;
        case 6: // op_imm
            this->rs1 = rs1;
            this->rd = rd;
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
            this->rd = rd;
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
            this->rs1 = rs1;
            this->rd = rd;
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
            this->rs1 = rs1;
            this->rd = rd;
            this->imm = int_of_binary(code.substr(4, 3) + code.substr(12, 5) + code.substr(22, 10));
            return;
        case 10: // jal
            this->type = Otype::o_jal;
            this->rd = rd;
            this->imm = int_of_binary(code.substr(4, 13) + code.substr(22, 10));
            return;
        case 11: // lui
            this->rd = rd;
            this->imm = int_of_binary("0" + code.substr(7, 10) + code.substr(22, 10));
            switch(funct){
                case 0: // lui
                    this->type = Otype::o_lui;
                    return;
                default: break;
            }
            break;
        case 12: // itof
            this->rs1 = rs1;
            this->rd = rd;
            switch(funct){
                case 0: // itof
                    this->type = Otype::o_fmvif;
                    return;
                default: break;
            }
            break;
        case 13: // ftoi
            this->rs1 = rs1;
            this->rd = rd;
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

// intをもとにするコンストラクタ
Operation::Operation(int i){
    std::stringstream code;
    code << std::bitset<32>(i);
    (*this) = Operation(code.str());
}

// 文字列に変換
std::string Operation::to_string(){
    switch(this->type){
        case Otype::o_add:
        case Otype::o_sub:
        case Otype::o_sll:
        case Otype::o_srl:
        case Otype::o_sra:
        case Otype::o_and:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd.value()) + ", x" + std::to_string(this->rs1.value()) + ", x" + std::to_string(this->rs2.value());
        case Otype::o_fadd:
        case Otype::o_fsub:
        case Otype::o_fmul:
        case Otype::o_fdiv:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd.value()) + ", f" + std::to_string(this->rs1.value()) + ", f" + std::to_string(this->rs2.value());
        case Otype::o_fsqrt:
        case Otype::o_fcvtif:
        case Otype::o_fcvtfi:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd.value()) + ", f" + std::to_string(this->rs1.value());
        case Otype::o_beq:
        case Otype::o_blt:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs1.value()) + ", x" + std::to_string(this->rs2.value()) + ", " + std::to_string(this->imm.value());
        case Otype::o_fbeq:
        case Otype::o_fblt:
            return string_of_otype(this->type) + " f" + std::to_string(this->rs1.value()) + ", f" + std::to_string(this->rs2.value()) + ", " + std::to_string(this->imm.value());
        case Otype::o_sw:
        case Otype::o_si:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs2.value()) + ", " + std::to_string(this->imm.value()) + "(x" + std::to_string(this->rs1.value()) + ")";
        case Otype::o_std:
            return string_of_otype(this->type) + " x" + std::to_string(this->rs1.value());
        case Otype::o_fsw:
            return string_of_otype(this->type) + " f" + std::to_string(this->rs2.value()) + ", " + std::to_string(this->imm.value()) + "(x" + std::to_string(this->rs1.value()) + ")";
        case Otype::o_addi:
        case Otype::o_slli:
        case Otype::o_srli:
        case Otype::o_srai:
        case Otype::o_andi:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd.value()) + ", x" + std::to_string(this->rs1.value()) + ", " + std::to_string(this->imm.value());
        case Otype::o_lw:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd.value()) + ", " + std::to_string(this->imm.value()) + "(x" + std::to_string(this->rs1.value()) + ")";
        case Otype::o_lre:
        case Otype::o_lrd:
        case Otype::o_ltf:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd.value());
        case Otype::o_flw:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd.value()) + ", " + std::to_string(this->imm.value()) + "(x" + std::to_string(this->rs1.value()) + ")";
        case Otype::o_jalr:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd.value()) + ", x" + std::to_string(this->rs1.value()) + ", " + std::to_string(this->imm.value());
        case Otype::o_jal:
        case Otype::o_lui:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd.value()) + ", " + std::to_string(this->imm.value());
        case Otype::o_fmvif:
            return string_of_otype(this->type) + " f" + std::to_string(this->rd.value()) + ", x" + std::to_string(this->rs1.value());
        case Otype::o_fmvfi:
            return string_of_otype(this->type) + " x" + std::to_string(this->rd.value()) + ", f" + std::to_string(this->rs1.value());
        default: std::exit(EXIT_FAILURE);
    }
}

// Otypeを文字列に変換
std::string string_of_otype(Otype t){
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
// stringに変換して取り出す
std::string Bit32::to_string(){
    return std::to_string(this->i);
    // switch(this->t){
    //     case Type::t_int:
    //         return std::to_string((*this).to_int());
    //     case Type::t_float:
    //         return std::to_string((*this).to_float());
    //     default: std::exit(EXIT_FAILURE);
    // }
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

// std::string Bit32::to_string(Stype t, const int len){
//     std::string res;
//     switch(t){
//         case Stype::t_dec: // 10進数
//             {
//                 std::ostringstream sout;
//                 sout << std::setfill('0') << std::setw(len) << this->v;
//                 res = sout.str();
//             }
//             break;
//         case Stype::t_bin: // 2進数
//             {
//                 int n = this->v;
//                 while (n > 0) {
//                     res.push_back('0' + (n & 1));
//                     n >>= 1;
//                 }
//                 std::reverse(res.begin(), res.end());
//                 res = "0b" + res;
//             }
//             break;
//         case Stype::t_hex: // 16進数
//             {
//                 std::ostringstream sout;
//                 sout << std::hex << std::setfill('0') << std::setw(len) << this->v;
//                 res = "0x" + sout.str();
//             }
//             break;
//         case Stype::t_float: // 浮動小数
//             {
//                 std::ostringstream sout;
//                 sout << std::setprecision(len) << this->to_float();
//                 res = sout.str();
//             }
//             break;
//         default:
//             std::cerr << head_error << "do not designate length with t_default/t_op" << std::endl;
//             std::exit(EXIT_FAILURE);
//     }

//     return res;
// }