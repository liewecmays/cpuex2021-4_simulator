#pragma once
#include "common.hpp"
#include <string>

Operation parse_op(std::string code); // 機械語命令をパースする
std::string string_of_op(Operation &op); // 命令を文字列に変換
