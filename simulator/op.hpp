#pragma once
#include "sim.hpp"
#include <string>

Operation parse_op(std::string code, int code_id, bool is_init);
void exec_op(Operation &op);
std::string string_of_op(Operation &op);