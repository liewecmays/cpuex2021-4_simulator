#include <fpu_test.hpp>
#include <common.hpp>
#include <util.hpp>
#include <fpu.hpp>
#include <string>
#include <vector>
#include <random>
#include <iostream>
#include <iomanip>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

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

int main(int argc, char *argv[]){
    unsigned int iteration = 0;
    std::vector<std::string> types;

    // コマンドライン引数をパース
    po::options_description opt("./fpu_test option");
	opt.add_options()
        ("help,h", "show help")
        ("type,t", po::value<std::vector<std::string>>()->multitoken(), "fpu type(s)")
        ("iter,i", po::value<unsigned int>()->default_value(100), "iteration number");
	po::variables_map vm;
    try{
        po::store(po::parse_command_line(argc, argv, opt), vm);
        po::notify(vm);
    }catch(po::error& e){
        std::cout << head_error << e.what() << std::endl;
        std::cout << opt << std::endl;
        std::exit(EXIT_FAILURE);
    }

	if(vm.count("help")){
        std::cout << opt << std::endl;
        std::exit(EXIT_FAILURE);
    }
    if(vm.count("type")){
        types = vm["type"].as<std::vector<std::string>>();
    }else{
        std::cout << head_error << "fpu types are required (use -t option)" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    if(vm.count("iter")){
        iteration = vm["iter"].as<unsigned int>();
    }

    // RAMの初期化
    init_ram();

    // 乱数の設定
    std::random_device rnd;
    std::mt19937 mt(rnd());

    // 検証に使う変数
    Bit32 x1, x2, y;
    float ieee;
    unsigned int i;
    bool has_error;

    for(auto type_string : types){
        Ftype t = ftype_of_string(type_string);
        has_error = false;
        i = 0;
        while(i<iteration){     
            x1.ui = mt();
            if(t != Ftype::o_itof && is_invalid(x1)) continue;
            if(has_two_args(t)){
                x2.ui = mt();
                if(t != Ftype::o_itof && is_invalid(x2)) continue;
            }else{
                x2 = Bit32(0);
            }

            y = calc_fpu(x1, x2, t);

            if(verify(x1, x2, y, t)){
                has_error = true;
                std::cout << "\x1b[1m" << type_string << ": \x1b[31mdoes not meet specification\x1b[0m" << std::endl;
                std::cout << std::setprecision(10) << "  x1\t= " << x1.f << "\t(" << x1.to_string(Stype::t_hex) << ")" << std::endl;
                if(has_two_args(t)){
                    std::cout << std::setprecision(10) << "  x2\t= " << x2.f << "\t(" << x2.to_string(Stype::t_hex) << ")" << std::endl;
                }
                std::cout << std::setprecision(10) << "  y\t= " << y.f << "\t(" << y.to_string(Stype::t_hex) << ")" << std::endl;
                ieee = calc_ieee(x1, x2, t);
                std::cout << std::setprecision(10) << "  ieee\t= " << ieee << "\t(" << Bit32(ieee).to_string(Stype::t_hex) << ")" << std::endl;
            }
            ++i;
        }
        if(!has_error){
            std::cout << "\x1b[1m" << type_string << ": \x1b[0mno error detected (during " << iteration << " iterations)" << std::endl;
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
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= max_of_4(std::abs(d_x1) * e_23.d, std::abs(d_x2) * e_23.d, std::abs(d_std) * e_23.d, e_126.d));
        case Ftype::o_fsub:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= max_of_4(std::abs(d_x1) * e_23.d, std::abs(d_x2) * e_23.d, std::abs(d_std) * e_23.d, e_126.d));
        case Ftype::o_fmul:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= std::max(std::abs(d_std) * e_22.d, e_126.d));
        case Ftype::o_fdiv:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && x2.f != 0.0f
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= std::max(std::abs(d_std) * e_20.d, e_126.d));
        case Ftype::o_fsqrt:
            return
                0.0f <= x1.f && x1.f < e127_32.f
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= std::max(d_std * e_20.d, e_126.d));
        case Ftype::o_itof:
            return
                is_invalid(y)
                || (!check_half(x1) && static_cast<float>(x1.i) != y.f)
                || (check_half(x1) && Bit32(static_cast<float>(x1.i)).ui != y.ui && Bit32(static_cast<float>(x1.i)).ui != y.ui - 1); // 0.5のとき切り上げるか切り上げないか
        case Ftype::o_ftoi:
            return
                -e31_32.f + 1 <= x1.f && x1.f <= e31_32.f - 1
                && static_cast<int>(std::nearbyint(x1.f)) != y.i;
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
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

float calc_ieee(Bit32 x1, Bit32 x2, Ftype t){
    switch(t){
        case Ftype::o_fadd:
            return x1.f + x2.f;
        case Ftype::o_fsub:
            return x1.f - x2.f;
        case Ftype::o_fmul:
            return x1.f * x2.f;
        case Ftype::o_fdiv:
            return x1.f / x2.f;
        case Ftype::o_fsqrt:
            return std::sqrt(x1.f);
        case Ftype::o_itof:
            return static_cast<float>(x1.i);
        case Ftype::o_ftoi:
            return std::nearbyint(x1.f);
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
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// 文字列をFtypeに変換
Ftype ftype_of_string(std::string s){
    if(s == "fadd"){
        return Ftype::o_fadd;
    }else if(s == "fsub"){
        return Ftype::o_fsub;
    }else if(s == "fmul"){
        return Ftype::o_fmul;
    }else if(s == "finv"){
        return Ftype::o_finv;
    }else if(s == "fdiv"){
        return Ftype::o_fdiv;
    }else if(s == "fsqrt"){
        return Ftype::o_fsqrt;
    }else if(s == "itof"){
        return Ftype::o_itof;
    }else if(s == "ftoi"){
        return Ftype::o_ftoi;
    }else{
        std::cerr << "no such fpu type: " << s << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

// itofの際に2つの解がありえるケースかどうかを判定
bool check_half(Bit32 x){
    ui abs_x = x.F.s == 1 ? ~x.ui + 1 : x.ui;
    bool res = false;
    if(abs_x != 0){
        ui cnt = 0;
        while((abs_x & 1) == 0){
            abs_x >>= 1;
            ++cnt;
        }
        if(cnt <= 6 && ((abs_x >> 24) & 1) == 1){
            abs_x >>= 25;
            cnt += 25;
            while(cnt < 32){
                if((abs_x & 1) == 1) break;
                abs_x >>= 1;
                ++cnt;
            }
            if(cnt == 32) res = true;
        }
    }
    return res;
}

inline double max_of_4(double d1, double d2, double d3, double d4){
    return std::max(d1, std::max(d2, std::max(d3, d4)));
}
