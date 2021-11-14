#include "util.hpp"
#include <string>

// ターミナルへの出力用
std::string head_error = "\033[2D\x1b[34m\x1b[1m\x1b[31mError: \x1b[0m";
std::string head_info = "\033[2D\x1b[34m\x1b[32mInfo: \x1b[0m";
std::string head_data = "\033[2D\x1b[34mData: \x1b[0m";
std::string head_warning = "\033[2D\x1b[33mWarning: \x1b[0m";
