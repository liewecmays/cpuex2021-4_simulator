#pragma once
#include "common.hpp"
#include <string>
#include <queue>
#include <boost/bimap/bimap.hpp>
#include <atomic>

/* typedef宣言 */
// boost::bimaps関連の略記
typedef boost::bimaps::bimap<std::string, unsigned int> bimap_t;
typedef bimap_t::value_type bimap_value_t;
typedef boost::bimaps::bimap<unsigned int, int> bimap_t2;
typedef bimap_t2::value_type bimap_value_t2;

/* クラスの定義 */
// スレッドの管理用フラグ
class cancel_flag{
    std::atomic<bool> signaled_{ false };
    public:
        void signal(){signaled_ = true;}
        bool operator!() {return !signaled_;}
};

// キャッシュのライン
struct cache_line{
    bool is_valid : 1;
    unsigned int tag : 31;
};

/* プロトタイプ宣言 */
void simulate(); // シミュレーションの本体処理
bool exec_command(std::string cmd); // デバッグモードのコマンドを認識して実行
void receive_data(); // データの受信
void send_data(cancel_flag&); // データの送信
void output_info(); // 情報の出力
void exec_op(); // 命令を実行し、PCを変化させる
unsigned int id_of_pc(unsigned int n); // PCから命令IDへの変換
int read_reg(int i); // 整数レジスタから読む
void write_reg(int i, int v); // 整数レジスタに書き込む
float read_reg_fp(int i); // 浮動小数点数レジスタから読む
Bit32 read_reg_fp_32(int i); // 浮動小数点数レジスタから読む(Bit32で)
void write_reg_fp(int i, float v); // 浮動小数点数レジスタに書き込む
void write_reg_fp(int i, int v);
void write_reg_fp_32(int i, Bit32 v); // 浮動小数点数レジスタに書き込む(Bit32のまま)
Bit32 read_memory(int i);
void write_memory(int i, Bit32 v);
bool check_cache(int w);
void print_reg(); // 整数レジスタの内容を表示
void print_reg_fp(); // 浮動小数点数レジスタの内容を表示
void print_memory(int start, int width); // 4byte単位でメモリの内容を出力
void print_queue(std::queue<Bit32> q, int n); // キューの表示
bool is_end(Operation op); // 終了時の命令かどうかを判定
