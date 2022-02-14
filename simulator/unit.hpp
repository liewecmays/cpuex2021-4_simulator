#pragma once
#include <common.hpp>
#include <iostream>
#include <queue>
#include <mutex>
#include <vector>
#include <algorithm>
#ifdef DETAILED
#include <sim.hpp>
#endif

/* レジスタ */
inline constexpr unsigned int reg_size = 32;
class Reg{
    private:
        Bit32 data[reg_size];
    public:
        constexpr Bit32 read_32(unsigned int i){
            return i == 0 ? Bit32(0) : this->data[i];
        }
        constexpr int read_int(unsigned int i){
            return i == 0 ? 0 : this->data[i].i;
        }
        constexpr float read_float(unsigned int i){
            return i == 0 ? 0.0f : this->data[i].f;
        }
        constexpr void write_32(unsigned int i, const Bit32& v){
            if(i != 0) this->data[i] = v;
        }
        constexpr void write_int(unsigned int i, int v){
            if(i != 0) this->data[i] = Bit32(v);
        }
        constexpr void write_float(unsigned int i, float v){
            if(i != 0) this->data[i] = Bit32(v);
        }
        void print(bool is_int, Stype t){
            std::string reg_type = is_int ? "x" : "f";
            for(unsigned int i=0; i<reg_size; ++i){
                std::cout << "\x1b[1m" << reg_type << i << "\x1b[0m:" << this->data[i].to_string(t) << " ";
                if(i % 4 == 3) std::cout << std::endl;
            }
        }
};


/* キャッシュ */
constexpr unsigned int addr_width = 25;
class Cache{
    private:
        unsigned int* tags;
        unsigned int index_width;
        unsigned int offset_width;
    public:
        unsigned long long accessed_times;
        unsigned long long hit_times;
        constexpr Cache(){ this->tags = {}; } // 宣言するとき用
        constexpr Cache(unsigned int index_width, unsigned int offset_width){
            this->tags = (unsigned int*) calloc(1 << index_width, sizeof(unsigned int));
            this->index_width = index_width;
            this->offset_width = offset_width;
        }
        constexpr unsigned int tag_width(){ return addr_width - (this->index_width + this->offset_width); }
        constexpr void read(unsigned int);
        constexpr void write(unsigned int);
};


inline constexpr void Cache::read(unsigned int addr){
    ++this->accessed_times;

    unsigned int index = take_bits(addr, this->offset_width, this->offset_width + this->index_width - 1);
    unsigned int tag = take_bits(addr, this->offset_width + this->index_width, addr_width - 1);

    if(this->tags[index] == tag){
        ++this->hit_times;
    }else{
        this->tags[index] = tag;
    }
}

inline constexpr void Cache::write(unsigned int addr){
    ++this->accessed_times;

    unsigned int index = take_bits(addr, this->offset_width, this->offset_width + this->index_width - 1);
    unsigned int tag = take_bits(addr, this->offset_width + this->index_width, addr_width - 1);
    
    if(this->tags[index] == tag) ++this->hit_times;
    this->tags[index] = tag;
}


/* メモリ */
constexpr int memory_border = 5000000;
class Memory{
    private:
        Bit32* data;
    public:
        constexpr Memory(){ this->data = {}; } // 宣言するとき用
        constexpr Memory(unsigned int size){ this->data = (Bit32*) calloc(size, sizeof(Bit32)); }
        constexpr Bit32 read(int w){ return this->data[w]; }
        constexpr void write(int w, const Bit32& v){ this->data[w] = v; }
        void print(int start, int width){
            for(int i=start; i<start+width; ++i){
                std::cout << "mem[" << i << "]: " << this->data[i].to_string() << std::endl;
            }
        }
};


/* 送受信用のキュー */
class TransmissionQueue{
    private:
        std::queue<Bit32> q;
        mutable std::mutex mutex;
    public:
        TransmissionQueue() = default;
        TransmissionQueue(const TransmissionQueue& original){
            this->q = original.q;
        }
        TransmissionQueue& operator=(const TransmissionQueue& original){
            TransmissionQueue copy;
            copy.q = original.q;
            return copy;
        }
        bool empty(){
            std::lock_guard<std::mutex> lock(this->mutex);
            return this->q.empty();
        }
        Bit32 pop(){
            std::lock_guard<std::mutex> lock(this->mutex);
            if(this->q.empty()){
                std::exit(EXIT_FAILURE);
            }else{
                Bit32 v = this->q.front();
                this->q.pop();
                return v;
            }
        }
        void push(const Bit32& v){
            std::lock_guard<std::mutex> lock(this->mutex);
            this->q.push(v);
        }
        void print(unsigned int size){
            std::queue<Bit32> copy = this->q;
            while(!copy.empty() && size > 0){
                std::cout << copy.front().to_string() << "; ";
                copy.pop();
                --size;
            }
            std::cout << std::endl;
        }
};

/* 分岐予測 */
class Gshare{
    private:
        unsigned int width;
        unsigned int global_history;
        int* branch_history_table;
    public:
        unsigned long long total_count;
        unsigned long long taken_count;
        unsigned long long correct_count;
        constexpr Gshare(unsigned int);
        constexpr void update(unsigned int, bool);
        void show_stats();
};

inline constexpr Gshare::Gshare(unsigned int width){
    this->width = width;
    this->global_history = 0;
    this->branch_history_table = (int*) calloc(1 << width, sizeof(int));
    for(unsigned int i=0; i<(1 << width); ++i) branch_history_table[i] = 1;
    this->total_count = 0;
    this->taken_count = 0;
    this->correct_count = 0;
}

inline constexpr void Gshare::update(unsigned int pc, bool taken){
    unsigned int index = (global_history ^ pc) & ((1 << this->width) - 1);
    ++total_count;
    if(taken) taken_count++;
    if((branch_history_table[index] >= 2) == taken) correct_count++;
    global_history = (global_history << 1) | (taken ? 1 : 0);
    branch_history_table[index] = std::clamp(branch_history_table[index] + (taken ? 1 : -1), 0, 3);
}

inline constexpr unsigned int gshare_width = 12;
