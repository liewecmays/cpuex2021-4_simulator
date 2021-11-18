#include <common.hpp>
#include <string>

union Bit64{
    unsigned long long u;
    double d;
};

enum class Ftype{
    o_fadd, o_fsub, o_fmul, o_finv, o_fdiv, o_fsqrt,
    o_itof, o_ftoi, o_floor
};

/* プロトタイプ宣言 */
bool verify(Bit32, Bit32, Bit32, Ftype); // 実装基準を満たしているかどうかの判定 (満たしていない場合true)
double calc_std(Bit32, Bit32, Ftype); // 比較対象の値を返す
Bit32 calc_fpu(Bit32, Bit32, Ftype); // FPUによる計算(1引数の場合第2引数は無視)
bool is_invalid(Bit32); // 有効でない浮動小数点数を判定
bool is_single_arg(Ftype); // 引数が1つかどうかを判定
std::string string_of_ftype(Ftype); // Ftypeを文字列に変換
double max_of_4(double, double, double, double);
