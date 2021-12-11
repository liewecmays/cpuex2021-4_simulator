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

    // RAMの初期化
    init_ram();

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
                    std::cout << head_info << error_count << " wrong case(s) was/were detected in \x1b[1m" << type_string << "\x1b[0m (pseudo-exhaustive test, with x2 random)" << std::endl;
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
                    if(t != Ftype::o_itof && is_invalid(x1)) continue;
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
                if(t != Ftype::o_itof && is_invalid(x1)) continue;
                if(has_two_args(t)){
                    x2.ui = mt();
                    if(t != Ftype::o_itof && is_invalid(x2)) continue;
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
                && static_cast<int>(std::round(x1.f)) != y.i;
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
            return 0.0; // 使わない
        case Ftype::o_ftoi:
            return 0.0; // 使わない
        default:
            std::cerr << "internal error" << std::endl;
            std::exit(EXIT_FAILURE);
    }
}

// IEEE754による計算
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
            return std::round(x1.f);
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

// 仕様を満たさない場合の出力
void print_result(Bit32 x1, Bit32 x2, Bit32 y, Ftype t){
    std::cout << "\x1b[1m" << string_of_ftype(t) << ": \x1b[31mdoes not meet specification\x1b[0m" << std::endl;
    std::cout << std::setprecision(10) << "  x1\t= " << x1.f << "\t(" << x1.to_string(Stype::t_hex) << ")" << std::endl;
    if(has_two_args(t)){
        std::cout << std::setprecision(10) << "  x2\t= " << x2.f << "\t(" << x2.to_string(Stype::t_hex) << ")" << std::endl;
    }
    std::cout << std::setprecision(10) << "  y\t= " << y.f << "\t(" << y.to_string(Stype::t_hex) << ")" << std::endl;
    float ieee = calc_ieee(x1, x2, t);
    std::cout << std::setprecision(10) << "  ieee\t= " << ieee << "\t(" << Bit32(ieee).to_string(Stype::t_hex) << ")" << std::endl;
}

inline double max_of_4(double d1, double d2, double d3, double d4){
    return std::max(d1, std::max(d2, std::max(d3, d4)));
}
