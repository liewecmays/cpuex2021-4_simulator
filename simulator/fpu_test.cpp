#include <common.hpp>
#include <fpu.hpp>
#include <random>
#include <iostream>
#include <iomanip>

using ui = unsigned int;
using ull = unsigned long long;

union Bit64{
    ull u;
    double d;
};

inline double max_of_4(double d1, double d2, double d3, double d4){
    return std::max(d1, std::max(d2, std::max(d3, d4)));
}

inline bool is_invalid(Bit32 b){
    return 
        (b.F.e == 0 && (b.F.e != 0 || b.F.m != 0)) // +0以外の非正規化数を除く
        || (b.F.e == 255); // infを除く
}

int main(){
    init_ram();

    std::random_device rnd;
    std::mt19937 mt(rnd());
    
    // 定数の設定
    Bit32 e127_32;
    Bit64 e_23, e_22, e_126, e127;
    e_23.u = 0x3e80000000000000;
    e_22.u = 0x3e90000000000000;
    e_126.u = 0x3810000000000000;
    e127.u = 0x47e0000000000000;
    e127_32.F.s = 0;
    e127_32.F.e = 254;
    e127_32.F.m = 0;

    Bit32 x1, x2, y;
    double d1, d2, d, d_true;
    for(int i=0; i<10; i++){     
        x1.ui = mt();
        if(is_invalid(x1)) break;
        x2.ui = mt();
        if(is_invalid(x2)) break;
        
        d1 = static_cast<double>(x1.f);
        d2 = static_cast<double>(x2.f);
        d_true = d1 + d2;

        y = fadd(x1, x2);
        d = static_cast<double>(y.f);

        if(is_invalid(y)){
            std::cout << "\x1b[1mfadd: \x1b[31minvalid value\x1b[0m" << std::endl;
        }else if(
            -e127_32.f < x1.f && x1.f < e127_32.f
            && -e127_32.f < x2.f && x2.f < e127_32.f
            && -e127.d < d_true && d_true < e127.d
            && std::abs(d - d_true) >= max_of_4(std::abs(d1) * e_23.d, std::abs(d2) * e_23.d, std::abs(d_true) * e_23.d, e_126.d)
        ){
            std::cout << "\x1b[1mfadd: \x1b[31mtoo large error\x1b[0m" << std::endl;
            std::cout << std::setprecision(10) << "  x1 = " << x1.f << "\t(" << x1.to_string(Stype::t_bin) << ")" << std::endl;
            std::cout << std::setprecision(10) << "  x2 = " << x2.f << "\t(" << x2.to_string(Stype::t_bin) << ")" << std::endl;
            std::cout << std::setprecision(10) << "  y  = " << y.f << "\t(" << y.to_string(Stype::t_bin) << ")" << std::endl;
        }
    }
}
