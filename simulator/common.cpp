#include "common.hpp"
#include <string>

// intを引数に取るコンストラクタ
Bit32::Bit32(int i){
    this->v = i;
    this->t = Type::t_int;
}

// intを引数に取るが、型を指定できるコンストラクタ
Bit32::Bit32(int i, Type t){
    this->v = i;
    this->t = t;
}

// floatを引数に取るコンストラクタ
Bit32::Bit32(float f){
    Int_float u;
    u.f = f;
    this->v = u.i;
    this->t = Type::t_float;
}

// intを取り出す
int Bit32::to_int(){
    return this->v;
}

// floatに変換して取り出す
float Bit32::to_float(){
    Int_float u;
    u.i = this->v;
    return u.f;
}

// stringに変換して取り出す
std::string Bit32::to_string(){
    switch(this->t){
        case Type::t_int:
            return std::to_string((*this).to_int());
        case Type::t_float:
            return std::to_string((*this).to_float());
    }
}
