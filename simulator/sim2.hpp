#pragma once
#include "common.hpp"
#include <string>
#include <vector>
#include <queue>
#include <boost/lockfree/queue.hpp>
#include <boost/bimap/bimap.hpp>

/* typedef宣言 */
// boost::bimaps関連の略記
typedef boost::bimaps::bimap<std::string, unsigned int> bimap_t;
typedef bimap_t::value_type bimap_value_t;
typedef boost::bimaps::bimap<unsigned int, int> bimap_t2;
typedef bimap_t2::value_type bimap_value_t2;

/* extern宣言 */
extern std::vector<Operation> op_list;
extern Reg reg_int;
extern Reg reg_fp;
extern Bit32 *memory;
extern unsigned int code_size;
extern std::queue<Bit32> receive_buffer;
extern boost::lockfree::queue<Bit32> send_buffer;
extern bool is_debug;
extern bool is_quick;
extern bool is_ieee;
extern bimap_t bp_to_id;
extern bimap_t label_to_id;
extern bimap_t2 id_to_line;
extern unsigned long long* op_type_count;

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
void receive_data(); // データの受信
void send_data(Cancel_flag&); // データの送信
bool exec_command(std::string); // デバッグモードのコマンドを認識して実行
// void output_info(); // 情報の出力
unsigned int id_of_pc(int); // PCから命令IDへの変換
Bit32 read_memory(int);
void write_memory(int, Bit32);
// bool check_cache(int);
void print_memory(int, int); // 4byte単位でメモリの内容を出力
void print_queue(std::queue<Bit32>, int); // キューの表示
unsigned long long op_count(); // 実効命令の総数を返す
void exit_with_output(std::string); // 実効情報を表示したうえで異常終了
