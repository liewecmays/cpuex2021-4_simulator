#pragma once
#include <string>

/* プロトタイプ宣言 */
int binary_stoi(std::string s); // 2進数を表す文字列から整数に変換
std::string binary_itos(int i); // 10進数を2進数の文字列へと変換
std::string binary_ftos(float f); // 浮動小数点数を2進数の文字列へと変換

/* extern宣言 */
extern std::string head_error;
extern std::string head_info;
extern std::string head_data;
