#include "util.hpp"
#include <string>

// ターミナルへの出力用
std::string head_error = "\033[2D\x1b[34m\x1b[1m\x1b[31mError: \x1b[0m";
std::string head_info = "\033[2D\x1b[34m\x1b[32mInfo: \x1b[0m";
std::string head_data = "\033[2D\x1b[34mData: \x1b[0m";

// 2進数を表す文字列から整数に変換
int binary_stoi(std::string s){
    int length = s.size(), res = 0, d;
    if(s[0] == '0'){ // 正数
        d = 1 << (length - 2);
        for(auto c : s.substr(1)){
            res += c == '1' ? d : 0;
            d = d >> 1;
        }
    }else{ // 負数
        d = 1 << 30;
        for(int i = 30; i>=0; i--){
            if(i >= length){ // 符号拡張
                res += d;
            }else{
                res += s[length - 1 - i] == '1' ? d : 0;
            }
            d = d >> 1;
        }
        res = res - (1 << 31);
    }
    return res;
}
