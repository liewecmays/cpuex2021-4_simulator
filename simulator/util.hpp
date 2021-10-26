#pragma once
#include "common.hpp"
#include <string>

/* プロトタイプ宣言 */
int int_of_binary(std::string s); // 2進数を表す文字列から整数に変換
std::string binary_of_int(int i); // 10進数を2進数の文字列へと変換
std::string binary_of_float(float f); // 浮動小数点数を2進数の文字列へと変換
std::string data_of_int(int i); // 整数を送信データへと変換
std::string data_of_float(int f); // 浮動小数点数を送信データへと変換
Bit32 bit32_of_data(std::string data); // 送信データをBit32へと変換

/* extern宣言 */
extern std::string head_error;
extern std::string head_info;
extern std::string head_data;
