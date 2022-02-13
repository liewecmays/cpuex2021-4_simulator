#include <common.hpp>
#include <string>

enum class Ftype{
    f_fadd, f_fsub, f_fmul, f_fdiv, f_fsqrt,
    f_itof, f_ftoi
};

/* プロトタイプ宣言 */
bool verify(const Bit32&, const Bit32&, const Bit32&, Ftype); // 実装基準を満たしているかどうかの判定 (満たしていない場合true)
double calc_std(const Bit32&, const Bit32&, Ftype); // 比較対象の値を返す
Bit32 calc_fpu(const Bit32&, const Bit32&, Ftype); // FPUによる計算(1引数の場合第2引数は無視)
float calc_ieee(const Bit32&, const Bit32&, Ftype); // IEEE754による計算
constexpr bool is_invalid(const Bit32&); // 有効でない浮動小数点数を判定
constexpr bool has_two_args(Ftype); // 引数が2つかどうかを判定
std::string string_of_ftype(Ftype); // Ftypeを文字列に変換
Ftype ftype_of_string(std::string); // 文字列をFtypeに変換
constexpr bool check_half(const Bit32& x); // itofの際に2つの解がありえるケースかどうかを判定
void print_result(Bit32, Bit32, Bit32, Ftype); // 仕様を満たさない場合の出力
constexpr double max_of_4(double, double, double, double);
