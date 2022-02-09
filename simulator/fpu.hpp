#pragma once
#include <common.hpp>
#include <cmath>
#include <optional>

using ui = unsigned int;
using ull = unsigned long long;

union Bit64{
    ull u;
    double d;
};

inline constexpr ui ram_size = 1024;
class Fpu{
    public:
        unsigned int ram_fsqrt_a[ram_size];
        unsigned int ram_fsqrt_b[ram_size];
        unsigned int ram_fsqrt_c[ram_size];
        unsigned int ram_fsqrt_d[ram_size];
        unsigned int ram_finv_a[ram_size];
        unsigned int ram_finv_b[ram_size];
        constexpr Fpu();
        constexpr Bit32 fadd(Bit32, Bit32);
        constexpr Bit32 fsub(Bit32, Bit32);
        constexpr Bit32 fmul(Bit32, Bit32);
        constexpr Bit32 fdiv(Bit32, Bit32);
        constexpr Bit32 fsqrt(Bit32);
        constexpr Bit32 itof(Bit32);
        constexpr Bit32 ftoi(Bit32);
    private:
        constexpr Bit32 finv(Bit32);
        constexpr std::optional<Bit32> close_path(Bit32, Bit32);
        constexpr std::optional<Bit32> special_path(Bit32, Bit32);
        constexpr Bit32 far_path(Bit32, Bit32);
};


/* 補助関数 */
// x[n]
constexpr inline ui take_bit(ui x, int n){
    return n >= 0 ? (x >> n) & 1 : 0;
}

// x[to:from]
constexpr inline ui take_bits(ui x, int from, int to){
    if(to >= 0){
        if(from >= 0){
            return (x >> from) & ((1 << (to - from + 1)) - 1);
        }else{
            return (x & ((1 << (to + 1)) - 1)) << (-from); // x[to:0] << (-from)
        }
    }else{
        return 0;
    }
}

// x[to:from]
constexpr inline ull take_bits(ull x, int from, int to){
    if(to >= 0){
        if(from >= 0){
            return (x >> from) & ((1 << (to - from + 1)) - 1);
        }else{
            return (x & ((1 << (to + 1)) - 1)) << (-from); // x[to:0] << (-from)
        }
    }else{
        return 0;
    }
}

// x[n] == 1
constexpr inline ui isset_bit(ui x, ui n){
    return ((x >> n) & 1) == 1;
}

constexpr inline ui count_bit(ui x){
    x = (x & 0x55555555) + (x >> 1 & 0x55555555);
    x = (x & 0x33333333) + (x >> 2 & 0x33333333);
    x = (x & 0x0f0f0f0f) + (x >> 4 & 0x0f0f0f0f);
    x = (x & 0x00ff00ff) + (x >> 8 & 0x00ff00ff);
    x = (x & 0x0000ffff) + (x >> 16 & 0x0000ffff);
    return x;
}

constexpr inline bool or_all(ui x){
    return count_bit(x) != 0;
}

constexpr inline bool and_all(ui x, ui len){ // 下lenビットを見る
    return count_bit(x) == len;
}

constexpr inline ui shift_mantissa(ui m, ui diff){
    return (diff >= 24) ? 0 : (m >> take_bits(diff, 0, 4));
}


/* class Fpu */
// 初期化
inline constexpr Fpu::Fpu(){
    ull x0, x1;
    for(ui i=0; i<ram_size; ++i){
        x0 = static_cast<ull>(
            std::nearbyint((std::sqrt(static_cast<double>(static_cast<ull>(1024+i) << 20))
            + std::sqrt(static_cast<double>(static_cast<ull>(1025+i) << 20))) / 2));
        this->ram_fsqrt_a[i] = static_cast<ui>(take_bits(x0 << 8, 0, 23));
        this->ram_fsqrt_b[i] = static_cast<ui>(take_bits((1ULL << 46) / (x0 << 8), 0, 23));
        this->ram_fsqrt_c[i] = static_cast<ui>(take_bits((x0 * 47453133ULL) >> 17, 0, 23));
        this->ram_fsqrt_d[i] = static_cast<ui>(take_bits((47453132ULL << 21) / (x0 << 8), 0, 23));
    }
    for(ui i=0; i<ram_size; ++i){
        x0 = (((1ULL << 46) / ((1024+i)*4096)) + ((1ULL << 46) / ((1025+i)*4096))) / 2;
        x1 = x0 * x0;
        this->ram_finv_a[i] = static_cast<ui>(take_bits(x1 >> 24, 0, 23));
        this->ram_finv_b[i] = static_cast<ui>(take_bits(x0, 0, 23));
    }
}

// 浮動小数点演算の定義
inline constexpr Bit32 Fpu::fadd(Bit32 x, Bit32 y){
    std::optional<Bit32> special_z = this->special_path(x, y);
    std::optional<Bit32> close_z = this->close_path(x, y);
    Bit32 far_z = this->far_path(x, y);
    
    if(special_z.has_value()){
        return(special_z.value());
    }else if(close_z.has_value()){
        return(close_z.value());
    }else{
        return far_z;
    }
}

inline constexpr Bit32 Fpu::fsub(Bit32 x, Bit32 y){
    return this->fadd(x, Bit32(((~y.F.s) << 31) + (y.F.e << 23) + y.F.m));
}

inline constexpr Bit32 Fpu::fmul(Bit32 x1, Bit32 x2){
    ui m1h = (1 << 12) + take_bits(x1.F.m, 11, 22);
    ui m2h = (1 << 12) + take_bits(x2.F.m, 11, 22);
    ui m1l = take_bits(x1.F.m, 0, 10);
    ui m2l = take_bits(x2.F.m, 0, 10);

    // stage 1
    ui hh = m1h * m2h;
    ui hl = m1h * m2l;
    ui lh = m1l * m2h;
    ui e3a = x1.F.e + x2.F.e + 129;
    ui s3 = x1.F.s ^ x2.F.s;
    ui zero1 = (x1.F.e == 0 || x2.F.e == 0) ? 1 : 0;
    
    // stage 2
    ui sum = hh + (hl >> 11) + (lh >> 11) + 2;
    ui e3b = e3a + 1;

    // assign
    ui e3 = !isset_bit(e3a, 8) ? 0 : ((isset_bit(sum, 25)) ? take_bits(e3b, 0, 7) : take_bits(e3a, 0, 7));
    ui m3 = isset_bit(sum, 25) ? take_bits(sum, 2, 24) : take_bits(sum, 1, 23);
    ui y = (zero1 == 1) ? 0 : ((e3 == 0) ? ((s3 << 31) + (1 << 23)) : ((s3 << 31) + (e3 << 23) + m3));
    
    return Bit32(y);
}

inline constexpr Bit32 Fpu::fdiv(Bit32 x1, Bit32 x2){
    ui e_diff = x2.F.e >= 253 ? 4 : 0;
    ui modified_x2 = (x2.F.s << 31) + ((x2.F.e - e_diff) << 23) + x2.F.m;

    Bit32 inv_x2 = this->finv(Bit32(modified_x2));
    Bit32 tmp = this->fmul(x1, inv_x2);

    ui y = tmp.F.e > e_diff ?
        (tmp.F.s << 31) + ((tmp.F.e - e_diff) << 23) + tmp.F.m
        : 0;
    return Bit32(y);
}

inline constexpr Bit32 Fpu::fsqrt(Bit32 x){
    // stage1
    ui m1 = x.F.m;
    
    // ram
    ui a = 0, b = 0;
    ui addr = take_bits(x.ui, 13, 22);
    if(isset_bit(x.ui, 23)){
        a = ram_fsqrt_a[addr];
        b = ram_fsqrt_b[addr];
    }else{
        a = ram_fsqrt_c[addr];
        b = ram_fsqrt_d[addr];
    }
    
    // assign
    ui hh = ((1 << 12) + take_bits(m1, 11, 22)) * take_bits(b, 11, 23);
    ui hl = ((1 << 12) + take_bits(m1, 11, 22)) * take_bits(b, 0, 10);
    ui lh = take_bits(m1, 0, 10) * take_bits(b, 11, 23);

    // stage2
    ui e2 = ((x.F.e - 127) >> 1) + 127;
    ui zero = (x.F.e == 0) ? 1 : 0;
    ui x_2 = a >> 1;
    ui hh2 = hh + 2;
    ui hllh = (hl >> 11) + (lh >> 11);
    
    // assign
    ui a_x = hh2 + hllh;
    ui m2 = x_2 + take_bits(a_x, 2, 25);
    ui y = zero == 1 ? 0 : (e2 << 23) + take_bits(m2, 0, 22);

    return Bit32(y);
}

inline constexpr Bit32 Fpu::itof(Bit32 x){
    // stage1
    ui abs_x = x.F.s == 1 ? ~x.ui + 1 : x.ui;

    // stage2
    ui tmp = 1 << 30;
    ui e1, m1, up;
    if(abs_x == 0){
        e1 = m1 = up = 0;
    }else{
        for(int i=1; i<32; ++i){
            if(i <= 31){
                if(tmp <= abs_x && abs_x < (tmp << 1)){
                    e1 = 158 - i;
                    m1 = take_bits(abs_x, 8-i, 30-i);
                    up = take_bit(abs_x, 7-i);
                    break;
                }
                tmp >>= 1;
            }else{ // default
                e1 = m1 = up = 0;
            }
        }   
    }

    // assign
    ui e2 = up == 0 ? e1 : (!and_all(m1, 23) ? e1 : e1 + 1);
    ui m2 = up == 0 ? m1 : (!and_all(m1, 23) ? m1 + 1 : 0);
    ui y = (x.F.s << 31) + (e2 << 23) + m2;

    return Bit32(y);
}

inline constexpr Bit32 Fpu::ftoi(Bit32 x){
    ui m1 = (1 << 23) + x.F.m;

    // stage1
    ui e1 = x.F.e;
    ui y1 = 0;
    if(150 <= e1 && e1 <= 158){
        y1 = m1 << (e1 - 150);
    }else{
        for(int i = 0; i < 24; ++i){ // 126, ..., 149
            if(e1 == static_cast<ui>(149 - i)){
                y1 = isset_bit(m1, i) ? ((m1 >> (i+1)) + 1) : (m1 >> (i+1));
                break;
            }
        }
    }

    // stage2
    ui y = x.F.s == 1 ? (~y1 + 1) : y1;
    
    return Bit32(y);
}

// 以下は内部的に必要な関数たち
inline constexpr Bit32 Fpu::finv(Bit32 x){
    // stage1
    ui m1 = (1 << 23) + x.F.m;

    // ram
    ui addr = take_bits(x.ui, 13, 22);
    ui a = ram_finv_a[addr];
    ui b = ram_finv_b[addr];

    // stage2
    ui e3 = x.F.e > 253 ? 0 : 253 - x.F.e;
    ull m2 = static_cast<ull>(m1) * static_cast<ull>(a);
    ull b2 = static_cast<ull>(b) << 1;

    // assign
    ull m3 = b2 - (m2 >> 23);
    ui y = (x.F.s << 31) + (e3 << 23) + static_cast<ui>(take_bits(m3, 0, 22));

    return Bit32(y);
}

inline constexpr std::optional<Bit32> Fpu::special_path(Bit32 x, Bit32 y){
    if(x.F.e == 0){
        return y;
    }else if(y.F.e == 0){
        return x;
    }else if((x.F.s != y.F.s) && (((x.F.e << 23) + x.F.m) == ((y.F.e << 23) + y.F.m))){
        return Bit32(0);
    }else{
        return std::nullopt;
    }
}

inline constexpr std::optional<Bit32> Fpu::close_path(Bit32 x, Bit32 y){
    // difference
    ui m_diff;
    if(x.F.e == y.F.e){
        m_diff = (x.F.m > y.F.m) ? (x.F.m - y.F.m) : (y.F.m - x.F.m);
    }else if(x.F.e > y.F.e){
        m_diff = ((1 << 23) + x.F.m) - ((1 << 22) + take_bits(y.F.m, 1, 22));
    }else{
        m_diff = ((1 << 23) + y.F.m) - ((1 << 22) + take_bits(x.F.m, 1, 22));
    }

    // LZC + shift
    ui shamt = 0, m_shifted;
    ui x_[6];
    x_[5] = m_diff;
    for(int i=4; i>=0; --i){
        if(!or_all(take_bits(x_[i+1], 23 - (1 << i) + 1, 23))){
            x_[i] = (x_[i+1] << (1 << i));
            shamt += (1 << i);
        }else{
            x_[i] = x_[i+1];
        }
    }
    m_shifted = x_[0];

    // retain the exponent of the bigger number
    ui e_ref = (x.F.e > y.F.e) ? x.F.e : y.F.e;

    // calculate z
    Bit32 z;
    if(e_ref > shamt){ // no underflow
        z.F.s = (((x.F.e << 23) + x.F.m) > ((y.F.e << 23) + y.F.m)) ? x.F.s : y.F.s;
        z.F.e = e_ref - shamt;
        z.F.m = take_bits(m_shifted, 0, 22);
    }
    
    // judge the close path
    bool opposite_sign = (x.F.s != y.F.s);
    int exp_signed_diff = static_cast<int>((1 << 8) + x.F.e) - static_cast<int>((1 << 8) + y.F.e);
    if(opposite_sign && ((exp_signed_diff == 0) || (exp_signed_diff == 1) || (exp_signed_diff == -1))){
        return z;
    }else{
        return std::nullopt;
    }
}

inline constexpr Bit32 Fpu::far_path(Bit32 x, Bit32 y){
    Bit32 z;

    // calculate the sign
    z.F.s = (x.F.e > y.F.e ? x.F.s : y.F.s);

    // calculate the mantissa
    ui mx_unshifted = (1 << 23) + x.F.m;
    ui mx_shifted = shift_mantissa(mx_unshifted, y.F.e - x.F.e);
    ui my_unshifted = (1 << 23) + y.F.m;
    ui my_shifted = shift_mantissa(my_unshifted, x.F.e - y.F.e);
    
    ui added_mantissa = (x.F.e > y.F.e) ? mx_unshifted + my_shifted : mx_shifted + my_unshifted;
    ui subtracted_mantissa = (x.F.e > y.F.e) ? mx_unshifted - my_shifted : my_unshifted - mx_shifted;

    ui mz_addition = (take_bit(added_mantissa, 24) == 1) ? take_bits(added_mantissa, 1, 23) : take_bits(added_mantissa, 0, 22);
    ui mz_subtraction = (take_bit(subtracted_mantissa, 23) == 1) ? take_bits(subtracted_mantissa, 0, 22) : (take_bits(subtracted_mantissa, 0, 21) << 1);

    z.F.m = (x.F.s == y.F.s) ? mz_addition : mz_subtraction;

    // calculate the exponent
    ui bigger_exp = (x.F.e > y.F.e) ? x.F.e : y.F.e;
    ui ez_addition = (take_bit(added_mantissa, 24) == 1) ? bigger_exp + 1 : bigger_exp;
    ui ez_subtraction = (take_bit(subtracted_mantissa, 23) == 1) ? bigger_exp : bigger_exp - 1;

    z.F.e = (x.F.s == y.F.s) ? ez_addition : ez_subtraction;

    return z;
}
