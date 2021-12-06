#include <fpu_test.hpp>
#include <common.hpp>
#include <fpu.hpp>
#include <string>
#include <random>
#include <iostream>
#include <iomanip>

using ui = unsigned int;
using ull = unsigned long long;

// 定数の設定
Bit32 e127_32 = {0x7f000000};
Bit32 e31_32 = {0x4f000000};
Bit64 e_23 = {0x3e80000000000000};
Bit64 e_22 = {0x3e90000000000000};
Bit64 e_20 = {0x3eb0000000000000};
Bit64 e_126 = {0x3810000000000000};
Bit64 e127 = {0x47e0000000000000};

int main(){
    // RAMの初期化
    init_ram();

    // 乱数の設定
    std::random_device rnd;
    std::mt19937 mt(rnd());

    Bit32 x1, x2, y;
    Ftype t = Ftype::o_fdiv;
    for(int i=0; i<1000000000; i++){     
        x1.ui = mt();
        if(is_invalid(x1)) break;
        if(has_two_args(t)){
            x2.ui = mt();
            if(is_invalid(x2)) break;
        }else{
            x2 = Bit32(0);
        }

        y = calc_fpu(x1, x2, t);

        if(is_invalid(y)){
            std::cout << "\x1b[1m" << string_of_ftype(t) << ": \x1b[31minvalid value\x1b[0m" << std::endl;
            std::cout << std::setprecision(10) << "  x1 = " << x1.f << "\t(" << x1.to_string(Stype::t_bin) << ")" << std::endl;
            if(has_two_args(t)){
                std::cout << std::setprecision(10) << "  x2 = " << x2.f << "\t(" << x2.to_string(Stype::t_bin) << ")" << std::endl;
            }
            std::cout << std::setprecision(10) << "  y  = " << y.f << "\t(" << y.to_string(Stype::t_bin) << ")" << std::endl;
        }else if(verify(x1, x2, y, t)){
            std::cout << "\x1b[1m" << string_of_ftype(t) << ": \x1b[31mdoes not meet specification\x1b[0m" << std::endl;
            std::cout << std::setprecision(10) << "  x1 = " << x1.f << "\t(" << x1.to_string(Stype::t_bin) << ")" << std::endl;
            if(has_two_args(t)){
                std::cout << std::setprecision(10) << "  x2 = " << x2.f << "\t(" << x2.to_string(Stype::t_bin) << ")" << std::endl;
            }
            std::cout << std::setprecision(10) << "  y  = " << y.f << "\t(" << y.to_string(Stype::t_bin) << ")" << std::endl;
        }
    }
}

// 実装基準を満たしているかどうかの判定 (満たしていない場合true)
bool verify(Bit32 x1, Bit32 x2, Bit32 y, Ftype t){
    double d_x1 = static_cast<double>(x1.f);
    double d_x2 = static_cast<double>(x2.f);
    double d_y = static_cast<double>(y.f);
    double d_std = calc_std(x1, x2, t);
    switch(t){
        case Ftype::o_fadd:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && std::abs(d_y - d_std) >= max_of_4(std::abs(d_x1) * e_23.d, std::abs(d_x2) * e_23.d, std::abs(d_std) * e_23.d, e_126.d);
        case Ftype::o_fsub:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && std::abs(d_y - d_std) >= max_of_4(std::abs(d_x1) * e_23.d, std::abs(d_x2) * e_23.d, std::abs(d_std) * e_23.d, e_126.d);
        case Ftype::o_fmul:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && std::abs(d_y - d_std) >= std::max(std::abs(d_std) * e_22.d, e_126.d);
        case Ftype::o_fdiv:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && x2.f != 0.0f
                && std::abs(d_y - d_std) >= std::max(std::abs(d_std) * e_20.d, e_126.d);
        case Ftype::o_fsqrt:
            return
                0.0f <= x1.f && x1.f < e127_32.f
                && std::abs(d_y - d_std) >= std::max(d_std * e_20.d, e_126.d);
        // case Ftype::o_itof:
        //     return ???
        // case Ftype::o_ftoi:
        //     return ???
        //         -e31_32.f + 1 <= x1.f && x1.f <= e31_32.f - 1
        //         &&
        case Ftype::o_floor:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && d_x1 != d_y
                && (/*std::nearbyint(d_y) != d_y // 整数ではない
                ||*/ d_y > d_x1 || d_x1 >= d_y + 1);
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// FPUによる計算(1引数の場合x2は無視)
Bit32 calc_fpu(Bit32 x1, Bit32 x2, Ftype t){
    switch(t){
        case Ftype::o_fadd:
            return fadd(x1, x2);
        case Ftype::o_fsub:
            return fsub(x1, x2);
        case Ftype::o_fmul:
            return fmul(x1, x2);
        case Ftype::o_fdiv:
            return fdiv(x1, x2);
        case Ftype::o_fsqrt:
            return fsqrt(x1);
        case Ftype::o_itof:
            return itof(x1);
        case Ftype::o_ftoi:
            return ftoi(x1);
        case Ftype::o_floor:
            return floor(x1);
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// 比較対象の値を返す
double calc_std(Bit32 x1, Bit32 x2, Ftype t){
    double d_x1 = static_cast<double>(x1.f);
    double d_x2 = static_cast<double>(x2.f);
    switch(t){
        case Ftype::o_fadd:
            return d_x1 + d_x2;
        case Ftype::o_fsub:
            return d_x1 - d_x2;
        case Ftype::o_fmul:
            return d_x1 * d_x2;
        case Ftype::o_fdiv:
            return d_x1 / d_x2;
        case Ftype::o_fsqrt:
            return std::sqrt(d_x1);
        case Ftype::o_itof:
            return static_cast<double>(x1.i);
        case Ftype::o_ftoi:
            return std::nearbyint(d_x1);
        case Ftype::o_floor:
            return 0.0; // 比較対象がいらない
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// 有効でない浮動小数点数を判定
inline bool is_invalid(Bit32 b){
    return 
        (b.F.e == 0 && (b.F.e != 0 || b.F.m != 0)) // +0以外の非正規化数を除く
        || (b.F.e == 255); // infを除く
}

// 引数が2つかどうかを判定
inline bool has_two_args(Ftype t){
    if(t == Ftype::o_fadd || t == Ftype::o_fsub || t == Ftype::o_fmul || t == Ftype::o_fdiv){
        return true;
    }else{
        return false;
    }
}

// Ftypeを文字列に変換
std::string string_of_ftype(Ftype t){
    switch(t){
        case Ftype::o_fadd: return "fadd";
        case Ftype::o_fsub: return "fsub";
        case Ftype::o_fmul: return "fmul";
        case Ftype::o_finv: return "finv";
        case Ftype::o_fdiv: return "fdiv";
        case Ftype::o_fsqrt: return "fsqrt";
        case Ftype::o_itof: return "itof";
        case Ftype::o_ftoi: return "ftoi";
        case Ftype::o_floor: return "floor";
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

inline double max_of_4(double d1, double d2, double d3, double d4){
    return std::max(d1, std::max(d2, std::max(d3, d4)));
}
