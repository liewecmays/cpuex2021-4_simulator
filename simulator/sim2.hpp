#pragma once
#include "common.hpp"
#include <string>
#include <array>
#include <queue>
#include <boost/bimap/bimap.hpp>

/* typedef宣言 */
// boost::bimaps関連の略記
typedef boost::bimaps::bimap<std::string, unsigned int> bimap_t;
typedef bimap_t::value_type bimap_value_t;
typedef boost::bimaps::bimap<unsigned int, int> bimap_t2;
typedef bimap_t2::value_type bimap_value_t2;

/* クラスの定義 */
class Hazard_info_man{
    public:
        unsigned int wb_addr;
        bool is_willing_but_not_ready_int;
        bool is_willing_but_not_ready_fp;
        bool cannot_accept;
};
class Hazard_info_fpn{
    public:
        unsigned int dest_addr;
        bool is_willing_but_not_ready;
        bool cannot_accept;
};
class IF_stage{
    public:
        std::array<unsigned int, 2> pc;
};
class ID_stage{
    public:
        std::array<unsigned int, 2> pc;
        std::array<Operation, 2> op;
        std::array<bool, 2> is_not_dispatched;
};
enum class EX_type{ EX_al1, EX_br1, EX_man, EX_fpn };
class EX_stage{
    public:
        std::array<unsigned int, 2> pc;
        std::array<Operation, 2> op;
        Hazard_info_man man_info;
        Hazard_info_fpn fpn_info;
};
class WB_stage{
    public:
        std::array<unsigned int, 2> pc;
        std::array<Operation, 2> op;
};
// 各時点の状態
class Configuration{
    public:
        IF_stage IF;
        ID_stage ID;
        EX_stage EX;
        WB_stage WB;
};


/* プロトタイプ宣言 */
void simulate(); // シミュレーションの本体処理
bool exec_command(std::string); // デバッグモードのコマンドを認識して実行
// void receive_data(); // データの受信
// void send_data(cancel_flag&); // データの送信
// void output_info(); // 情報の出力

void advance_clock(); // クロックを1つ分先に進める
bool intra_hazard_detector(); // 同時発行される命令の間のハザード検出
bool inter_hazard_detector(unsigned int); // 同時発行されない命令間のハザード検出
bool iwp_hazard_detector(unsigned int); // 書き込みポート数が不十分な場合のハザード検出

unsigned int id_of_pc(unsigned int); // PCから命令IDへの変換
int read_reg(unsigned int); // 整数レジスタから読む
Bit32 read_reg_32(unsigned int); // 整数レジスタから読む(Bit32で)
void write_reg(unsigned int, int); // 整数レジスタに書き込む
void write_reg_32(unsigned int, Bit32); // 整数レジスタに書き込む(Bit32で)
float read_reg_fp(unsigned int); // 浮動小数点数レジスタから読む
Bit32 read_reg_fp_32(unsigned int); // 浮動小数点数レジスタから読む(Bit32で)
void write_reg_fp(unsigned int, float); // 浮動小数点数レジスタに書き込む
void write_reg_fp(unsigned int, int);
void write_reg_fp_32(unsigned int, Bit32); // 浮動小数点数レジスタに書き込む(Bit32で)
Bit32 read_memory(int);
void write_memory(int, Bit32);
bool check_cache(int);
void print_reg(); // 整数レジスタの内容を表示
void print_reg_fp(); // 浮動小数点数レジスタの内容を表示
void print_memory(int, int); // 4byte単位でメモリの内容を出力
// void print_queue(std::queue<Bit32>, int); // キューの表示
bool is_end(Operation); // 終了時の命令かどうかを判定
void exit_with_output(std::string); // 実効情報を表示したうえで異常終了
