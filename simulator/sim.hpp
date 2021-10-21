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

union Int_float{
    int i;
    float f;
};

extern std::vector<Operation> op_list;
extern std::vector<int> reg_list;
extern std::vector<float> reg_fp_list;
extern std::vector<Int_float> memory;

extern std::string head;
extern std::string error;
extern std::string info;
