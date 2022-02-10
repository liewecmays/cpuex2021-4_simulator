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
class Cancel_flag{
    std::atomic<bool> signaled_{ false };
    public:
        void signal(){signaled_ = true;}
        bool operator!() {return !signaled_;}
};

/* プロトタイプ宣言 */
void simulate(); // シミュレーションの本体処理
bool exec_command(std::string); // デバッグモードのコマンドを認識して実行
void receive_data(); // データの受信
void send_data(Cancel_flag&); // データの送信
void output_info(); // 情報の出力
void exec_op(); // 命令を実行し、PCを変化させる
Bit32 read_memory(int);
void write_memory(int, Bit32);
bool check_cache(int);
void print_reg(); // 整数レジスタの内容を表示
void print_reg_fp(); // 浮動小数点数レジスタの内容を表示
void print_memory(int, int); // 4byte単位でメモリの内容を出力
void print_queue(std::queue<Bit32>, int); // キューの表示
bool constexpr is_end(Operation); // 終了時の命令かどうかを判定
void exit_with_output(std::string); // 実効情報を表示したうえで異常終了
