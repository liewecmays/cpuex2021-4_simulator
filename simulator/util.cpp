#include "util.hpp"
#include "sim.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

unsigned int id_of_pc(unsigned int n){
    if(n % 4 == 0){
        return n / 4;
    }else{
        std::cerr << error << "error with program counter" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

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
float read_reg_fp(int i){
    return i == 0 ? 0 : reg_fp_list[i];
}

// 浮動小数レジスタに書く
void write_reg_fp(int i, float v){
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
                    break;
                case 1: // fsub
                    res += "fsub ";
                    break;
                case 2: // fmul
                    res += "fmul ";
                    break;
                case 3: // fdiv
                    res += "fdiv ";
                    break;
                default: return "";
            }
            res += ("rs1=f" + std::to_string(op.rs1) + ", ");
            res += ("rs2=f" + std::to_string(op.rs2) + ", ");
            res += ("rd=f" + std::to_string(op.rd));
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
        case 3: // store
            switch(op.funct){
                case 0: // sw
                    res += "sw ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=x" + std::to_string(op.rs2) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        // case 4: // store_fp
        //     switch(op.funct){
        //         case 0: // sw
        //             res += "fsw "
        //             break;
        //         default: return "";
        //     }
        //     return res;
        case 5: // op_imm
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
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 6: // load
            switch(op.funct){
                case 0: // lw
                    res += "lw ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        // case 7: // load_fp
        //     switch(op.funct){
        //         case 0: // lw
        //             res += "flw ";
        //             break;
        //         default: return "";
        //     }
        //     return res;
        case 8: // jalr
            res = "jalr ";
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 9: // jal
            res = "jal ";
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 10: // long_imm
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
        default: return "";
    }
}

void print_reg(){
    for(int i=0; i<32; i++){
        std::cout << "\x1b[1mx" << i << "\x1b[0m:" << std::ends;
        if(i < 10) std::cout << " " << std::ends;
        std::cout.setf(std::ios::hex, std::ios::basefield);
        std::cout.fill('0');
        std::cout << std::setw(8) << reg_list[i] << " " << std::ends;
        std::cout.setf(std::ios::dec, std::ios::basefield);
        if(i % 4 == 3) std::cout << std::endl;
    }
    return;
}

void print_reg_fp(){
    for(int i=0; i<32; i++){
        std::cout << "\x1b[1mf" << i << "\x1b[0m:" << std::ends;
        if(i < 10) std::cout << " " << std::ends;
        std::cout.setf(std::ios::hex, std::ios::basefield);
        std::cout.fill('0');
        std::cout << std::setw(8) << *((int*)&(reg_fp_list[i])) << " " << std::ends;
        std::cout.setf(std::ios::dec, std::ios::basefield);
        if(i % 4 == 3) std::cout << std::endl;
    }
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

// 終了時の無限ループ命令(jal x0, 0)であるかどうかを判定
bool is_end(Operation op){
    return (op.opcode == 9) && (op.funct == -1) && (op.rs1 = -1) && (op.rs2 == -1) && (op.rd == 0) && (op.imm == 0);
}