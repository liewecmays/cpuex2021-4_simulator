#pragma once
#include <string>
#include <vector>

struct Operation{
    int opcode;
    int funct;
    int rs1;
    int rs2;
    int rd;
    int imm;
};

extern std::vector<Operation> op_list; // 命令のリスト(PC順)
extern std::vector<int> reg_list; // 整数レジスタのリスト
extern std::vector<float> reg_fp_list; //
extern std::vector<int> memory; // メモリ領域
