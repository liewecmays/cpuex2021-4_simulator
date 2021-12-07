#pragma once
#include "common.hpp"

union Bit64{
    unsigned long long u;
    double d;
};

/* 定数 */
inline constexpr int ram_size = 1024;

/* extern宣言 */
extern unsigned int ram_fsqrt_a[];
extern unsigned int ram_fsqrt_b[];
extern unsigned int ram_fsqrt_c[];
extern unsigned int ram_fsqrt_d[];
extern unsigned int ram_finv_a[];
extern unsigned int ram_finv_b[];

/* プロトタイプ宣言 */
void init_ram(); // RAMの初期化
Bit32 fadd(Bit32, Bit32);
Bit32 fsub(Bit32, Bit32);
Bit32 fmul(Bit32, Bit32);
Bit32 finv(Bit32);
Bit32 fdiv(Bit32, Bit32);
Bit32 fsqrt(Bit32);
Bit32 itof(Bit32);
Bit32 ftoi(Bit32);
