#include "common.hpp"
#include <string>

Bit32::Bit32(int i){
    this->v = i;
    this->t = Type::t_int;
}

Bit32::Bit32(float f){
    Int_float u;
    u.f = f;
    this->v = u.i;
    this->t = Type::t_float;
}

int Bit32::to_int(){
    return this->v;
}

float Bit32::to_float(){
    Int_float u;
    u.i = this->v;
    return u.f;
}

std::string Bit32::to_string(){
    switch(this->t){
        case Type::t_int:
            return std::to_string((*this).to_int());
        case Type::t_float:
            return std::to_string((*this).to_float());
    }
}
