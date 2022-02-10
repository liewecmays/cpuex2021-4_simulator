#pragma once
#include <common.hpp>

/* レジスタ */
class Reg{
    private:
        Bit32 val[32];
    public:
        constexpr Bit32 read_32(unsigned int i){
            return i == 0 ? Bit32(0) : this->val[i];
        };
        constexpr int read_int(unsigned int i){
            return i == 0 ? 0 : this->val[i].i;
        };
        constexpr float read_float(unsigned int i){
            return i == 0 ? 0.0f : this->val[i].f;
        };
        constexpr void write_32(unsigned int i, Bit32 v){
            if(i != 0) this->val[i] = v;
        }
        constexpr void write_int(unsigned int i, int v){
            if(i != 0) this->val[i] = Bit32(v);
        };
        constexpr void write_float(unsigned int i, float v){
            if(i != 0) this->val[i] = Bit32(v);
        };
};

/* キャッシュ */
struct Cache_line{
    bool is_valid : 1;
    unsigned int tag : 31;
};
class Cache{
    private:
        Cache_line* lines;
        unsigned int index_width;
        unsigned int offset_width;
    public:
        unsigned long long read_times;
        unsigned long long hit_times;
        constexpr Cache(){ this->lines = {}; } // 宣言するとき用
        constexpr Cache(unsigned int index_width, unsigned int offset_width){
            this->lines = (Cache_line*) calloc(1 << index_width, sizeof(Cache_line));
            this->index_width = index_width;
            this->offset_width = offset_width;
        }
        constexpr void read(unsigned int);
        constexpr void write(unsigned int);
};

inline constexpr void Cache::read(unsigned int addr){
    ++this->read_times;

    unsigned int tag = ((addr) >> (this->index_width + this->offset_width)) & ((1 << (32 - (this->index_width + this->offset_width))) - 1);
    unsigned int index = ((addr) >> this->offset_width) & ((1 << this->index_width) - 1);

    Cache_line line = this->lines[index];
    if(line.tag == tag){
        if(line.is_valid){
            ++this->hit_times;
        }else{
            line.is_valid = true;
            this->lines[index] = line;
        }
    }else{
        line.is_valid = true;
        line.tag = tag;
        this->lines[index] = line;
    }
}

inline constexpr void Cache::write(unsigned int addr){
    unsigned int tag = ((addr) >> (this->index_width + this->offset_width)) & ((1 << (32 - (this->index_width + this->offset_width))) - 1);
    unsigned int index = ((addr) >> this->offset_width) & ((1 << this->index_width) - 1);
    this->lines[index].is_valid = true;
    this->lines[index].tag = tag;
}
