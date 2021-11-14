#include "common.hpp"
#include "fpu.hpp"
#include <random>
#include <iostream>
#include <iomanip>

using ui = unsigned int;
using ull = unsigned long long;

int main(){
    init_ram();

    std::random_device seed_gen;
    std::default_random_engine engine(seed_gen());

    std::uniform_real_distribution<> dist(0, 100);
    Bit32 u1, u2;

    for(int i=0; i<10; ++i){        
        u1.f = dist(engine);
        // u2.f = dist(engine);
        // std::cout << std::setprecision(10) << "fmul(" << u1.f << ", " << u2.f << ") = " << fmul(u1, u2).f << " (" << u1.f * u2.f << ")" << std::endl;
        std::cout << std::setprecision(10) << "finv(" << u1.f << ") = " << finv(u1).f << " (" << 1 / u1.f << ")" << std::endl;
    }
}
