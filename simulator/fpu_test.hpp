#include <common.hpp>
#include <string>

enum class Ftype{
    o_fadd, o_fsub, o_fmul, o_fdiv, o_fsqrt,
    o_itof, o_ftoi
};

/* プロトタイプ宣言 */
bool verify(Bit32, Bit32, Bit32, Ftype); // 実装基準を満たしているかどうかの判定 (満たしていない場合true)
double calc_std(Bit32, Bit32, Ftype); // 比較対象の値を返す
Bit32 calc_fpu(Bit32, Bit32, Ftype); // FPUによる計算(1引数の場合第2引数は無視)
float calc_ieee(Bit32, Bit32, Ftype); // IEEE754による計算
bool is_invalid(Bit32); // 有効でない浮動小数点数を判定
bool has_two_args(Ftype); // 引数が2つかどうかを判定
std::string string_of_ftype(Ftype); // Ftypeを文字列に変換
Ftype ftype_of_string(std::string); // 文字列をFtypeに変換
bool check_half(Bit32 x); // itofの際に2つの解がありえるケースかどうかを判定
void print_result(Bit32, Bit32, Bit32, Ftype); // 仕様を満たさない場合の出力
double max_of_4(double, double, double, double);
