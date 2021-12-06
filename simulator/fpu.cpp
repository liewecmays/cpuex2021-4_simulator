#include <fpu.hpp>
#include <common.hpp>
#include <cmath>
#include<iostream>

using ui = unsigned int;
using ull = unsigned long long;

// RAM
unsigned int ram_fsqrt[ram_size];
unsigned int ram_finv_a[ram_size];
unsigned int ram_finv_b[ram_size];


/* 補助関数 */
// x[n]
inline ui take_bit(ui x, int n){
    return n >= 0 ? (x >> n) & 1 : 0;
}

// x[to:from]
inline ui take_bits(ui x, int from, int to){
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
inline ull take_bits(ull x, int from, int to){
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
inline ui isset_bit(ui x, ui n){
    return ((x >> n) & 1) == 1;
}

inline ui count_bit(ui x){
    x = (x & 0x55555555) + (x >> 1 & 0x55555555);
    x = (x & 0x33333333) + (x >> 2 & 0x33333333);
    x = (x & 0x0f0f0f0f) + (x >> 4 & 0x0f0f0f0f);
    x = (x & 0x00ff00ff) + (x >> 8 & 0x00ff00ff);
    x = (x & 0x0000ffff) + (x >> 16 & 0x0000ffff);
    return x;
}

inline ui or_all(ui x){
    return count_bit(x) != 0;
}

inline ui and_all(ui x, ui len){ // 下lenビットを見る
    return count_bit(x) == len;
}


/* RAMの初期化 */
void init_ram(){
    for(int i=0; i<ram_size; ++i){
        int x0 = static_cast<int>((std::sqrt((1024+i) << 20) + std::sqrt((1025+i) << 20)) / 2);
        ram_fsqrt[i] = x0 << 8;
    }
    for(int i=0; i<ram_size; ++i){
        ull x0 = (((1LU << 46) / ((1024+i)*4096)) + ((1LU << 46) / ((1025+i)*4096))) / 2;
        ull x1 = x0 * x0;
        ram_finv_a[i] = static_cast<ui>(x1 >> 24);
        ram_finv_b[i] = static_cast<ui>(x0);
    }
}


/* 浮動小数点演算の定義 */
Bit32 fadd(Bit32 x1, Bit32 x2){
    ui big =
        x1.F.e == x2.F.e ?
            (x1.F.m >= x2.F.m ? 1 : 0) :
            (x1.F.e > x2.F.e ? 1 : 0);
    ui x1_m = (1 << 23) + x1.F.m;
    ui x2_m = (1 << 23) + x2.F.m;
    
    // stage1
    ui s1 = big == 1 ? x1.F.s : x2.F.s;
    ui e1 = big == 1 ? x1.F.e : x2.F.e;
    ui calc = x1.F.s ^ x2.F.s;
    ui big_x =
        big == 1 ?
            (x1.F.e == 0 ? 0 : x1_m) :
            (x2.F.e == 0 ? 0 : x2_m);
    ui small_x =
        big == 1 ?
            (x2.F.e == 0 ? 0 : x2_m >> (x1.F.e - x2.F.e)) :
            (x1.F.e == 0 ? 0 : x1_m >> (x2.F.e - x1.F.e));
    
    // stage2
    ui m2 = calc == 1 ? take_bits(((1 << 24) + big_x) - ((1 << 24) + small_x), 0, 24) : take_bits(((1 << 24) + big_x) + ((1 << 24) + small_x), 0, 24);

    // stage3
    ui y = 0;
    for(ui i = 0; i <= 24; ++i){
        if(isset_bit(m2, 24 - i)){
            y = (s1 << 31) + ((e1 + (1 - i)) << 23) + take_bits(m2, 1-i, 23-i);
            break;
        }
    }
    
    return Bit32(y);
}

Bit32 fsub(Bit32 x1, Bit32 x2){
    return fadd(x1, ((~x2.F.s) << 31) + (x2.F.e << 23) + x2.F.m);
}

Bit32 fmul(Bit32 x1, Bit32 x2){
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
    
    // stage 2
    ui sum = hh + (hl >> 11) + (lh >> 11) + 2;
    ui e3b = e3a + 1;

    // assign
    ui e3 = !isset_bit(e3a, 8) ? 0 : ((isset_bit(sum, 25)) ? take_bits(e3b, 0, 7) : take_bits(e3a, 0, 7));
    ui m3 = isset_bit(sum, 25) ? take_bits(sum, 2, 24) : take_bits(sum, 1, 23);
    ui y = (e3 == 0) ? 0 : ((s3 << 31) + (e3 << 23) + m3);
    
    return Bit32(y);
}

Bit32 finv(Bit32 x){
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
    ull m3 = (b2 - (m2 >> 23));
    ui y = (x.F.s << 31) + (e3 << 23) + static_cast<ui>(take_bits(m3, 0, 22));

    return Bit32(y);
}

Bit32 fdiv(Bit32 x1, Bit32 x2){
    ui e_diff = x2.F.e >= 253 ? 4 : 0;
    ui modified_x2 = (x2.F.s << 31) + ((x2.F.e - e_diff) << 23) + x2.F.m;

    Bit32 inv_x2 = finv(Bit32(modified_x2));
    ui tmp = fmul(x1, inv_x2).ui;

    ui y = take_bits(tmp, 23, 30) >= e_diff ?
        (take_bit(tmp, 31) << 31) + ((take_bits(tmp, 23, 30) - e_diff) << 23) + take_bits(tmp, 0, 22)
        : 0;
    return Bit32(y);
}

Bit32 fsqrt(Bit32 x){
    // stage1
    ull m1 = (1LU << 46) + (static_cast<ull>(x.F.m) << 23);
    
    // ram
    ui addr = take_bits(x.ui, 13, 22);
    ui a = ram_fsqrt[addr];
    
    // stage2
    ui e2 = x.F.e;
    ull x0_2 = a >> 1;
    ull m2 = m1 / (a << 1);
    
    // assign
    ull m3 = isset_bit(e2, 0) ? x0_2 + m2 : (((x0_2 + m2) * 0xb504f3) >> 23);
    ui e3 = ((e2 - 127) >> 1) + 127;
    ui y = (e2 == 0) ? 0 : (e3 << 23) + static_cast<ui>(take_bits(m3, 0, 22));

    return Bit32(y);
}

Bit32 floor(Bit32 x){
    // stage1
    ui e1 = x.F.e;
    ui m1 = x.F.m;
    ui m2=0, over=0;
    ui tmp = 0;
    ui all_zero = 0;
    for(ui i=0; i<23; ++i){ // 127, 128, ..., 149
        if(e1 == 127 + i){
            m2 = (1 << 23) + (m1 & tmp);
            over = x.F.s << (23-i);
            all_zero = ~or_all(take_bits(m1, 0, 22-i));
            break;
        }
        tmp += 1 << (22 - i);
    }
    if(e1 > 149){
        m2 = m1;
        over = all_zero = 0;
    }else if(e1 <= 126){
        m2 = over = all_zero = 0;
    }
    ui s2 = x.F.s;
    
    // assign
    ui m3 = (all_zero == 1) ? m2 : m2 + over;
    ui y = e1 <= 126 ?
            (s2 == 0 ? 0 : 0xbf800000) :
            (isset_bit(m3, 24) ?
                ((s2 << 31) + ((e1 + 1) << 23) + take_bits(m3, 23, 1)) :
                ((s2 << 31) + (e1 << 23) + take_bits(m3, 0, 22)));
    
    return Bit32(y);
}

Bit32 itof(Bit32 x){
    // stage1
    ui abs_x = x.F.s == 1 ? ~x.ui + 1 : x.ui;

    // stage2
    ui tmp = 1 << 30;
    ui e1, m1, up;
    if(abs_x == 0){
        e1 = m1 = up = 0;
    }else{
        for(int i=1; i<32; ++i){
            if(i < 31){
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

Bit32 ftoi(Bit32 x){
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
