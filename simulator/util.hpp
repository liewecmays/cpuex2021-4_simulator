#pragma once
#include "common.hpp"
#include <string>
#include <iostream>
#include <bitset>

/* extern宣言 */
extern std::string head_error;
extern std::string head_info;
extern std::string head_data;
extern std::string head_warning;

/* プロトタイプ宣言 */
// 2進数を表す文字列から整数に変換
inline int int_of_binary(std::string s){
    if(s == ""){
        std::cerr << head_error << "invalid input to 'int_of_binary'" << std::endl;
        std::exit(EXIT_FAILURE);
    }

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

// 10進数を2進数の文字列へと変換
inline std::string binary_of_int(int i){
    std::bitset<32> bs(i);
    return bs.to_string();
}

// 浮動小数点数を2進数の文字列へと変換
inline std::string binary_of_float(float f){
    std::bitset<32> bs(Bit32(f).i);
    return bs.to_string();
}

// 整数を送信データへと変換
inline std::string data_of_int(int i){
    return binary_of_int(i);
}

// 浮動小数点数を送信データへと変換
inline std::string data_of_float(float f){
    return binary_of_float(f);
}

// 2進数の文字列を送信データへと変換
inline std::string data_of_binary(std::string s){
    if(s.size() <= 32){
        while(s.size() < 32){
            s = "0" + s;
        }
    }else{
        std::cerr << head_error << "invalid input to 'data_of_binary'" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return s;
}

// 送信データをBit32へと変換
inline Bit32 bit32_of_data(std::string data){
    if(data.size() < 32){
        data = "0" + data; // 8ビットのデータなので、先頭に0を追加して変換
    }
    return Bit32(int_of_binary(data));
    // if(data[0] == 'i'){ // int
    //     return Bit32(int_of_binary(data.substr(1,32)));
    // }else if(data[0] == 'f'){ // float
    //     return Bit32(int_of_binary(data.substr(1,32)));
    // }else{
    //     std::cerr << head_error << "invalid input to 'bit32_of_data'" << std::endl;
    //     std::exit(EXIT_FAILURE);
    // }
}
