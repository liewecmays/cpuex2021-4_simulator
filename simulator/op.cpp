#include "op.hpp"
#include "common.hpp"
#include "util.hpp"
#include <string>
#include <iostream>

// 機械語命令をパースする
Operation parse_op(std::string code){
    Operation op;
    int opcode, funct, rs1, rs2, rd;    
    opcode = std::stoi(code.substr(0, 4), 0, 2);
    funct = std::stoi(code.substr(4, 3), 0, 2);
    rs1 = std::stoi(code.substr(7, 5), 0, 2);
    rs2 = std::stoi(code.substr(12, 5), 0, 2);
    rd = std::stoi(code.substr(17, 5), 0, 2);
    
    op.opcode = opcode;
    switch(opcode){
        case 0: // op
        case 1: // op_fp
            op.funct = funct;
            op.rs1 = rs1;
            op.rs2 = rs2;
            op.rd = rd;
            op.imm = -1;
            break;
        case 2: // branch
        case 3: // branch_fp
        case 4: // store
        case 5: // store_fp
            op.funct = funct;
            op.rs1 = rs1;
            op.rs2 = rs2;
            op.rd = -1;
            op.imm = binary_stoi(code.substr(17, 15));
            break;
        case 6: // op_imm
        case 7: // load
        case 8: // load_fp
            op.funct = funct;
            op.rs1 = rs1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = binary_stoi(code.substr(12, 5) + code.substr(22, 10));
            break;
        case 9: // jalr
            op.funct = -1;
            op.rs1 = rs1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = binary_stoi(code.substr(4, 3) + code.substr(12, 5) + code.substr(22, 10));
            break;
        case 10: // jal
            op.funct = -1;
            op.rs1 = -1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = binary_stoi(code.substr(4, 13) + code.substr(22, 10));
            break;
        case 11: // long_imm
            op.funct = funct;
            op.rs1 = -1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = binary_stoi("0" + code.substr(7, 10) + code.substr(22, 10));
            break;
        case 12: // itof
        case 13: // ftoi
            op.funct = funct;
            op.rs1 = rs1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = -1;
            break;
        default:
            std::cerr << head_error << "could not parse the code" << std::endl;
            std::exit(EXIT_FAILURE);
    }

    return op;
}

// 命令を文字列に変換
std::string string_of_op(Operation &op){
    std::string res = "";
    switch(op.opcode){
        case 0: // op
            switch(op.funct){
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
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=x" + std::to_string(op.rs2) + ", ");
            res += ("rd=x" + std::to_string(op.rd));
            return res;
        case 1: // op_fp
            switch(op.funct){
                case 0: // fadd
                    res += "fadd ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 1: // fsub
                    res += "fsub ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 2: // fmul
                    res += "fmul ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 3: // fdiv
                    res += "fdiv ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 4: // fsqrt
                    res += "fsqrt ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        case 2: // branch
            switch(op.funct){
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
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=x" + std::to_string(op.rs2) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 3: // branch_fp
            switch(op.funct){
                case 0: // fbeq
                    res += "fbeq ";
                    break;
                case 1: // fblt
                    res += "fblt ";
                    break;
            }
            res += ("rs1=f" + std::to_string(op.rs1) + ", ");
            res += ("rs2=f" + std::to_string(op.rs2) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 4: // store
            switch(op.funct){
                case 0: // sw
                    res += "sw ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=x" + std::to_string(op.rs2) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 1: // si
                    res += "si ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=x" + std::to_string(op.rs2) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 2: // std
                    res += "std ";
                    res += ("rs2=x" + std::to_string(op.rs2));
                    break;
                default: return "";
            }
            
            return res;
        case 5: // store_fp
            switch(op.funct){
                case 0: // fsw
                    res += "fsw ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 1: // fstd
                    res += "fstd ";
                    res += ("rs1=f" + std::to_string(op.rs1));
                    break;
                default: return "";
            }
            return res;
        case 6: // op_imm
            switch(op.funct){
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
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 7: // load
            switch(op.funct){
                case 0: // lw
                    res += "lw ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rd=x" + std::to_string(op.rd) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 1: // lre
                    res += "lre ";
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                case 2: // lrd
                    res += "lrd ";
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                case 3: // ltf
                    res += "ltf ";
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        case 8: // load_fp
            switch(op.funct){
                case 0: // lw
                    res += "flw ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rd=f" + std::to_string(op.rd) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 2: // flrd
                    res += "flrd ";
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        case 9: // jalr
            res = "jalr ";
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 10: // jal
            res = "jal ";
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 11: // long_imm
            switch(op.funct){
                case 0: // lui
                    res += "lui ";
                    break;
                case 1: // auipc
                    res += "auipc ";
                    break;
                default: return "";
            }
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 12: // itof
            switch(op.funct){
                case 0: // fmv.i.f
                    res += "fmv.i.f ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 5: // fcvt.i.f
                    res += "fcvt.i.f ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        case 13: // ftoi
            switch(op.funct){
                case 0: // fmv.f.i
                    res += "fmv.f.i ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                case 6: // fcvt.f.i
                    res += "fcvt.f.i ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        default: return "";
    }
}
