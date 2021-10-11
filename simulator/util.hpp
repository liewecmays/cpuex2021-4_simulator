#pragma once
#include "sim.hpp"
#include <string>

int binary_stoi(std::string s);
std::string string_of_op(Operation &op);
void print_op_list();
void print_reg();
void print_reg_fp();
void print_memory(int start, int end);
void print_memory_word(int start, int end);