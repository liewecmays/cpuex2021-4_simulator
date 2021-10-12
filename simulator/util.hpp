#pragma once
#include "sim.hpp"
#include <string>

int read_reg(int i);
void write_reg(int i, int v);
int read_reg_fp(int i);
void write_reg_fp(int i, int v);
int binary_stoi(std::string s);
std::string string_of_op(Operation &op);
void print_op_list();
void print_reg();
void print_reg_fp();
void print_memory(int start, int end);
void print_memory_word(int start, int end);