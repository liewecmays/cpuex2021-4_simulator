#pragma once
#include "sim.hpp"
#include <string>

unsigned int id_of_pc(unsigned int n);
int read_reg(int i);
void write_reg(int i, int v);
float read_reg_fp(int i);
void write_reg_fp(int i, float v);
int binary_stoi(std::string s);
std::string string_of_op(Operation &op);
void print_reg();
void print_reg_fp();
void print_memory(int start, int width);
bool is_end(Operation op);
