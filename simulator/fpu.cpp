#include <fpu.hpp>

using ui = unsigned int;
using ull = unsigned long long;

unsigned int ram_fsqrt_a[ram_size];
unsigned int ram_fsqrt_b[ram_size];
unsigned int ram_fsqrt_c[ram_size];
unsigned int ram_fsqrt_d[ram_size];
unsigned int ram_finv_a[ram_size];
unsigned int ram_finv_b[ram_size];

// 初期化
void init_ram(){
    ull x0, x1;
    for(ui i=0; i<ram_size; ++i){
        x0 = static_cast<ull>(
            std::nearbyint((std::sqrt(static_cast<double>(static_cast<ull>(1024+i) << 20))
            + std::sqrt(static_cast<double>(static_cast<ull>(1025+i) << 20))) / 2));
        ram_fsqrt_a[i] = static_cast<ui>(take_bits(x0 << 8, 0, 23));
        ram_fsqrt_b[i] = static_cast<ui>(take_bits((1ULL << 46) / (x0 << 8), 0, 23));
        ram_fsqrt_c[i] = static_cast<ui>(take_bits((x0 * 47453133ULL) >> 17, 0, 23));
        ram_fsqrt_d[i] = static_cast<ui>(take_bits((47453132ULL << 21) / (x0 << 8), 0, 23));
    }
    for(ui i=0; i<ram_size; ++i){
        x0 = (((1ULL << 46) / ((1024+i)*4096)) + ((1ULL << 46) / ((1025+i)*4096))) / 2;
        x1 = x0 * x0;
        ram_finv_a[i] = static_cast<ui>(take_bits(x1 >> 24, 0, 23));
        ram_finv_b[i] = static_cast<ui>(take_bits(x0, 0, 23));
    }
}
