#include "util.hpp"
#include "sim.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

// PCから命令IDへの変換(4の倍数になっていない場合エラー)
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

// 4byteごとに出力
void print_memory(int start, int width){
    for(int i=start; i<start+width; i++){
        std::cout.setf(std::ios::hex, std::ios::basefield);
        std::cout.fill('0');
        std::cout << "mem[" << i << "]: " << memory[i].i << std::endl;
        std::cout.setf(std::ios::dec, std::ios::basefield);
    }
    return;
}

// 終了時の無限ループ命令(jal x0, 0)であるかどうかを判定
bool is_end(Operation op){
    return (op.opcode == 10) && (op.funct == -1) && (op.rs1 = -1) && (op.rs2 == -1) && (op.rd == 0) && (op.imm == 0);
}