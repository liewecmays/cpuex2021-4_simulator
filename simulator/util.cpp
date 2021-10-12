#include "util.hpp"
#include "sim.hpp"
#include <string>
#include <vector>
#include <iostream>

// 整数レジスタを読む
int read_reg(int i){
    return i == 0 ? 0 : reg_list[i];
}

// 整数レジスタに書く
void write_reg(int i, int v){
    if (i != 0) reg_list[i] = v;
    return;
}

// 浮動小数レジスタを読む
int read_reg_fp(int i){
    return i == 0 ? 0 : reg_fp_list[i];
}

// 浮動小数レジスタに書く
void write_reg_fp(int i, int v){
    if (i != 0) reg_fp_list[i] = v;
    return;
}

// 2進数を表す文字列から整数に変換
int binary_stoi(std::string s){
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

// 命令を文字列に変換
std::string string_of_op(Operation &op){
    std::string res;
    switch(op.opcode){
        case 0: // op
            res = "op[";
            switch(op.funct){
                case 0: // add
                    res += "add] ";
                    break;
                case 1: // sub
                    res += "sub] ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=x" + std::to_string(op.rs2) + ", ");
            res += ("rd=x" + std::to_string(op.rd));
            return res;
        case 1: // op_fp
            res = "op_fp[";
            switch(op.funct){
                case 0: // fadd
                    res += "fadd], ";
                    break;
                case 1: // fsub
                    res += "fsub], ";
                    break;
                case 2: // fmul
                    res += "fmul], ";
                    break;
                case 3: // fdiv
                    res += "fdiv], ";
                    break;
                default: return "";
            }
            res += ("rs1=f" + std::to_string(op.rs1) + ", ");
            res += ("rs2=f" + std::to_string(op.rs2) + ", ");
            res += ("rd=f" + std::to_string(op.rd));
            return res;
        case 2: // branch
            res = "branch[";
            switch(op.funct){
                case 0: // beq
                    res += "beq], ";
                    break;
                case 1: // blt
                    res += "blt], ";
                    break;
                case 2: // belt
                    res += "belt], ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=x" + std::to_string(op.rs2) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 3: // store
            res = "store[";
            switch(op.funct){
                case 0: // sw
                    res += "sw], ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=x" + std::to_string(op.rs2) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 4: // store_fp
            res = "store_fp[";
            switch(op.funct){
                case 0: // sw
                    res += "sw], ";
                    memory[op.rs1 + op.imm] = reg_list[op.rs2];
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=f" + std::to_string(op.rs2) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 5: // op_imm
            res = "op_imm[";
            switch(op.funct){
                case 0: // addi
                    res += "addi], ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 6: // load
            res = "load[";
            switch(op.funct){
                case 0: // lw
                    res += "lw], ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 7: // load_fp
            res = "load_fp[";
            switch(op.funct){
                case 0: // lw
                    res += "lw], ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=f" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 8: // jalr
            res = "jalr, ";
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 9: // jal
            res = "jal, ";
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 10: // lui
            res = "lui, imm=" + std::to_string(op.imm);
        default: return "";
    }
}

void print_op_list(){
    for(unsigned int i=0; i<op_list.size(); i++){
        std::cout << i << ": " << string_of_op(op_list[i]) << std::endl;
    }
    return;
}

void print_reg(){
    for(int i=0; i<10; i++)
        std::cout << "x" << i << ":" << reg_list[i] << ", " << std::ends;
    std::cout << std::endl;
    return;
}

void print_reg_fp(){
    for(int i=0; i<5; i++)
        std::cout << "f" << i << ":" << reg_fp_list[i] << std::endl;
    return;
}

void print_memory(int start, int end){
    for(int i=start; i<end; i++)
        std::cout << i << ":" << memory[i] << std::endl;
    return;
}

// 4byteごとに出力
void print_memory_word(int start, int end){
    int v;
    for(int i=start; i<end; i++){
        v = 0;
        for(int j=0; j<4; j++){
            v += memory[4 * i + j] << (8 * j);  
        }
        std::cout << "mem[" << i << "]: " << v << std::endl;
    }
    return;
}
