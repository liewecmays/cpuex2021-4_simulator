#pragma once
#include <common.hpp>
#include <iostream>
#include <queue>
#include <mutex>
#include <optional>

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
        constexpr void write_32(unsigned int i, const Bit32& v){
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
constexpr unsigned int addr_width = 25;
struct Cache_line{
    public:
        unsigned int dirty : 1;
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
        constexpr unsigned int tag_width(){ return addr_width - (this->index_width + this->offset_width); }
        constexpr void read(unsigned int);
        constexpr void write(unsigned int);
};


inline constexpr void Cache::read(unsigned int addr){
    ++this->read_times;

    unsigned int index = take_bits(addr, this->offset_width, this->index_width);
    unsigned int tag = take_bits(addr, this->offset_width + this->index_width, this->tag_width());

    if(this->lines[index].tag == tag){
        ++this->hit_times;
    }else{
        this->lines[index].tag = tag;
    }
}

inline constexpr void Cache::write(unsigned int addr){
    unsigned int index = take_bits(addr, this->offset_width, this->index_width);
    unsigned int tag = take_bits(addr, this->offset_width + this->index_width, this->tag_width());
    
    this->lines[index].dirty = 1; // todo
    this->lines[index].tag = tag;
}

/* メモリ */
class Memory{
    private:
        Bit32* data;
    public:
        constexpr Memory(){ this->data = {}; }; // 宣言するとき用
        constexpr Memory(unsigned int size){ this->data = (Bit32*) calloc(size, sizeof(Bit32)); };
        constexpr Bit32 read(int w){ return this->data[w]; };
        constexpr void write(int w, const Bit32& v){ this->data[w] = v; };
        void print(int start, int width){
            for(int i=start; i<start+width; ++i){
                std::cout << "mem[" << i << "]: " << this->data[i].to_string() << std::endl;
            }
        };
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
        std::optional<Bit32> pop(){
            std::lock_guard<std::mutex> lock(this->mutex);
            if(this->q.empty()){
                return std::nullopt;
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
