#include <fpu_test.hpp>
#include <common.hpp>
#include <fpu.hpp>
#include <string>
#include <vector>
#include <random>
#include <iostream>
#include <iomanip>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
using enum Ftype;
using enum Stype;

using ui = unsigned int;
using ull = unsigned long long;

// FPU
Fpu fpu;

// 定数の設定
inline constexpr Bit32 e127_32 = {0x7f000000};
inline constexpr Bit32 e31_32 = {0x4f000000};
inline constexpr Bit64 e_23 = {0x3e80000000000000};
inline constexpr Bit64 e_22 = {0x3e90000000000000};
inline constexpr Bit64 e_20 = {0x3eb0000000000000};
inline constexpr Bit64 e_126 = {0x3810000000000000};
inline constexpr Bit64 e127 = {0x47e0000000000000};

// 制御用の変数
std::vector<std::string> types; // 検証対象の命令
unsigned int iteration = 0; // 反復回数
bool is_exhaustive = false; // (擬似)全数検査モード
constexpr ui iter_ratio = 10; // 擬似全数検査で、片方の引数1回の反復でもう一方の引数を検査する回数

int main(int argc, char *argv[]){
    // コマンドライン引数をパース
    po::options_description opt("./fpu_test option");
	opt.add_options()
        ("help,h", "show help")
        ("type,t", po::value<std::vector<std::string>>()->multitoken(), "fpu type(s)")
        ("iter,i", po::value<unsigned int>()->default_value(100), "iteration number")
        ("exh,e", "(pseudo) exhaustive mode");
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
    if(vm.count("exh")){
        is_exhaustive = true;
    }


    // 乱数の設定
    std::random_device rnd;
    std::mt19937 mt(rnd());

    // 検証に使う変数
    Bit32 x1, x2, y;
    ui error_count;

    for(auto type_string : types){
        Ftype t = ftype_of_string(type_string);
        error_count = 0;
        if(is_exhaustive){
            if(has_two_args(t)){
                // 2変数の場合は擬似的に全数検査
                // 第1引数を全数にして検査
                bool is_first = true;
                ui j = 0;
                for(ui i=0; i<=0xffffffff; ++i){
                    if(i == 0){
                        if(is_first){
                            is_first = false;
                        }else{
                            break; // 1周したら終了
                        }
                    }
                    x1.ui = i;
                    if(is_invalid(x1)) continue;
                    while(j < iter_ratio){
                        x2.ui = mt();
                        if(is_invalid(x2)) continue;
                        y = calc_fpu(x1, x2, t);
                        if(verify(x1, x2, y, t)){
                            ++error_count;
                            print_result(x1, x2, y, t);
                        }
                        ++j;
                    }
                    j = 0;
                    if(i % 1'000'000'000 == 999'999'999) std::cout << head_info << static_cast<ull>(i+1) * iter_ratio << " cases of \x1b[1m" << type_string << "\x1b[0m have been verified" << std::endl;
                }
                if(error_count == 0){
                    std::cout << head_info << "there was no wrong case detected in \x1b[1m" << type_string << "\x1b[0m (pseudo-exhaustive test, with x2 random)" << std::endl;
                }else{
                    std::cout << head_info << error_count << " wrong case(s) was/were detected in \x1b[1m" << type_string << "\x1b[0m (pseudo-exhaustive test, with x2 random)" << std::endl;
                }
                // 第2引数を全数にして検査
                is_first = true;
                j = 0;
                error_count = 0;
                for(ui i=0; i<=0xffffffff; ++i){
                    if(i == 0){
                        if(is_first){
                            is_first = false;
                        }else{
                            break; // 1周したら終了
                        }
                    }
                    x2.ui = i;
                    if(is_invalid(x2)) continue;
                    while(j < iter_ratio){
                        x1.ui = mt();
                        if(is_invalid(x1)) continue;
                        y = calc_fpu(x1, x2, t);
                        if(verify(x1, x2, y, t)){
                            ++error_count;
                            print_result(x1, x2, y, t);
                        }
                        ++j;
                    }
                    j = 0;
                    if(i % 1'000'000'000 == 999'999'999) std::cout << head_info << static_cast<ull>(i+1) * iter_ratio << " cases of \x1b[1m" << type_string << "\x1b[0m have been verified" << std::endl;
                }
                if(error_count == 0){
                    std::cout << head_info << "there was no wrong case detected in \x1b[1m" << type_string << "\x1b[0m (pseudo-exhaustive test, with x1 random)" << std::endl;
                }else{
                    std::cout << head_info << error_count << " wrong case(s) was/were detected in \x1b[1m" << type_string << "\x1b[0m (pseudo-exhaustive test, with x1 random)" << std::endl;
                }
            }else{
                // 1変数の場合は本当に全数検査
                bool is_first = true;
                for(ui i=0; i<=0xffffffff; ++i){
                    if(i == 0){
                        if(is_first){
                            is_first = false;
                        }else{
                            break; // 1周したら終了
                        }
                    }
                    x1.ui = i;
                    if(t != f_itof && is_invalid(x1)) continue;
                    y = calc_fpu(x1, x2, t);
                    if(verify(x1, x2, y, t)){
                        ++error_count;
                        print_result(x1, x2, y, t);
                    }
                    if(i % 1'000'000'000 == 999'999'999) std::cout << head_info << (i+1) << " cases of \x1b[1m" << type_string << "\x1b[0m have been verified" << std::endl;
                }
                if(error_count == 0){
                    std::cout << head_info << "there was no wrong case detected in \x1b[1m" << type_string << "\x1b[0m (exhaustive test)" << std::endl;
                }else{
                    std::cout << head_info << error_count << " wrong case(s) was/were detected in \x1b[1m" << type_string << "\x1b[0m (exhaustive test)" << std::endl;
                }
            }
        }else{
            unsigned int i = 0;
            while(i<iteration){     
                x1.ui = mt();
                if(t != f_itof && is_invalid(x1)) continue;
                if(has_two_args(t)){
                    x2.ui = mt();
                    if(t != f_itof && is_invalid(x2)) continue;
                }

                y = calc_fpu(x1, x2, t);
                if(verify(x1, x2, y, t)){
                    ++error_count;
                    print_result(x1, x2, y, t);
                }
                ++i;
            }
            if(error_count == 0){
                std::cout << head_info << "there was no wrong case detected in \x1b[1m" << type_string << "\x1b[0m (" << iteration << " iterations)" << std::endl;
            }else{
                std::cout << head_info << error_count << " wrong case(s) was/were detected in \x1b[1m" << type_string << "\x1b[0m (" << iteration << " iterations)" << std::endl;
            }
        }
    }
}

// 実装基準を満たしているかどうかの判定 (満たしていない場合true)
bool verify(const Bit32& x1, const Bit32& x2, const Bit32& y, Ftype t){
    double d_x1 = static_cast<double>(x1.f);
    double d_x2 = static_cast<double>(x2.f);
    double d_y = static_cast<double>(y.f);
    double d_std = calc_std(x1, x2, t);
    switch(t){
        case f_fadd:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= max_of_4(std::abs(d_x1) * e_23.d, std::abs(d_x2) * e_23.d, std::abs(d_std) * e_23.d, e_126.d));
        case f_fsub:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= max_of_4(std::abs(d_x1) * e_23.d, std::abs(d_x2) * e_23.d, std::abs(d_std) * e_23.d, e_126.d));
        case f_fmul:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && !(std::abs(d_y) == e_126.d && d_y * d_std > 0.0 && std::abs(d_y - d_std) == e_126.d) // yが2^-126のときの情報落ちに対応
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= std::max(std::abs(d_std) * e_22.d, e_126.d));
        case f_fdiv:
            return
                -e127_32.f < x1.f && x1.f < e127_32.f
                && -e127_32.f < x2.f && x2.f < e127_32.f
                && -e127.d < d_std && d_std < e127.d
                && x2.f != 0.0f
                && !(std::abs(d_y) == e_126.d && d_y * d_std > 0.0 && std::abs(d_y - d_std) == e_126.d) // yが2^-126のときの情報落ちに対応
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= std::max(std::abs(d_std) * e_20.d, e_126.d));
        case f_fsqrt:
            return
                0.0f <= x1.f && x1.f < e127_32.f
                && (is_invalid(y)
                || std::abs(d_y - d_std) >= std::max(d_std * e_20.d, e_126.d));
        case f_itof:
            return
                is_invalid(y)
                || (!check_half(x1) && static_cast<float>(x1.i) != y.f)
                || (check_half(x1) && Bit32(static_cast<float>(x1.i)).ui != y.ui && Bit32(static_cast<float>(x1.i)).ui != y.ui - 1); // 0.5のとき切り上げるか切り上げないか
        case f_ftoi:
            return
                -e31_32.f + 1 <= x1.f && x1.f <= e31_32.f - 1
                && static_cast<int>(std::round(x1.f)) != y.i;
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// FPUによる計算(1引数の場合x2は無視)
Bit32 calc_fpu(const Bit32& x1, const Bit32& x2, Ftype t){
    switch(t){
        case f_fadd:
            return fpu.fadd(x1, x2);
        case f_fsub:
            return fpu.fsub(x1, x2);
        case f_fmul:
            return fpu.fmul(x1, x2);
        case f_fdiv:
            return fpu.fdiv(x1, x2);
        case f_fsqrt:
            return fpu.fsqrt(x1);
        case f_itof:
            return fpu.itof(x1);
        case f_ftoi:
            return fpu.ftoi(x1);
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// 比較対象の値を返す
double calc_std(const Bit32& x1, const Bit32& x2, Ftype t){
    double d_x1 = static_cast<double>(x1.f);
    double d_x2 = static_cast<double>(x2.f);
    switch(t){
        case f_fadd:
            return d_x1 + d_x2;
        case f_fsub:
            return d_x1 - d_x2;
        case f_fmul:
            return d_x1 * d_x2;
        case f_fdiv:
            return d_x1 / d_x2;
        case f_fsqrt:
            return std::sqrt(d_x1);
        case f_itof:
            return 0.0; // 使わない
        case f_ftoi:
            return 0.0; // 使わない
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// IEEE754による計算
float calc_ieee(const Bit32& x1, const Bit32& x2, Ftype t){
    switch(t){
        case f_fadd:
            return x1.f + x2.f;
        case f_fsub:
            return x1.f - x2.f;
        case f_fmul:
            return x1.f * x2.f;
        case f_fdiv:
            return x1.f / x2.f;
        case f_fsqrt:
            return std::sqrt(x1.f);
        case f_itof:
            return static_cast<float>(x1.i);
        case f_ftoi:
            return std::round(x1.f);
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// 有効でない浮動小数点数を判定
inline constexpr bool is_invalid(const Bit32& b){
    return 
        (b.F.e == 0 && (b.F.e != 0 || b.F.m != 0)) // +0以外の非正規化数を除く
        || (b.F.e == 255); // infを除く
}

// 引数が2つかどうかを判定
inline constexpr bool has_two_args(Ftype t){
    if(t == f_fadd || t == f_fsub || t == f_fmul || t == f_fdiv){
        return true;
    }else{
        return false;
    }
}

// Ftypeを文字列に変換
std::string string_of_ftype(Ftype t){
    switch(t){
        case f_fadd: return "fadd";
        case f_fsub: return "fsub";
        case f_fmul: return "fmul";
        case f_fdiv: return "fdiv";
        case f_fsqrt: return "fsqrt";
        case f_itof: return "itof";
        case f_ftoi: return "ftoi";
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// 文字列をFtypeに変換
Ftype ftype_of_string(std::string s){
    if(s == "fadd"){
        return f_fadd;
    }else if(s == "fsub"){
        return f_fsub;
    }else if(s == "fmul"){
        return f_fmul;
    }else if(s == "fdiv"){
        return f_fdiv;
    }else if(s == "fsqrt"){
        return f_fsqrt;
    }else if(s == "itof"){
        return f_itof;
    }else if(s == "ftoi"){
        return f_ftoi;
    }else{
        std::cerr << "no such fpu type: " << s << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

// itofの際に2つの解がありえるケースかどうかを判定
inline constexpr bool check_half(const Bit32& x){
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

// 仕様を満たさない場合の出力
void print_result(Bit32 x1, Bit32 x2, Bit32 y, Ftype t){
    std::cout << "\x1b[1m" << string_of_ftype(t) << ": \x1b[31mdoes not meet specification\x1b[0m" << std::endl;
    std::cout << std::setprecision(10) << "  x1\t= " << x1.f << "\t(" << x1.to_string(t_hex) << ")" << std::endl;
    if(has_two_args(t)){
        std::cout << std::setprecision(10) << "  x2\t= " << x2.f << "\t(" << x2.to_string(t_hex) << ")" << std::endl;
    }
    std::cout << std::setprecision(10) << "  y\t= " << y.f << "\t(" << y.to_string(t_hex) << ")" << std::endl;
    float ieee = calc_ieee(x1, x2, t);
    std::cout << std::setprecision(10) << "  ieee\t= " << ieee << "\t(" << Bit32(ieee).to_string(t_hex) << ")" << std::endl;
}

inline constexpr double max_of_4(double d1, double d2, double d3, double d4){
    return std::max(d1, std::max(d2, std::max(d3, d4)));
}
