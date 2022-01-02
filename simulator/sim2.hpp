#pragma once
#include "common.hpp"
#include "config.hpp"
#include <string>
#include <vector>
#include <queue>
#include <boost/bimap/bimap.hpp>

/* typedef宣言 */
// boost::bimaps関連の略記
typedef boost::bimaps::bimap<std::string, unsigned int> bimap_t;
typedef bimap_t::value_type bimap_value_t;
typedef boost::bimaps::bimap<unsigned int, int> bimap_t2;
typedef bimap_t2::value_type bimap_value_t2;

/* extern宣言 */
extern std::vector<Operation> op_list;
extern bool is_debug;
extern bool is_ieee;
extern bimap_t2 id_to_line;
extern unsigned long long* op_type_count;

/* プロトタイプ宣言 */
void simulate(); // シミュレーションの本体処理
bool exec_command(std::string); // デバッグモードのコマンドを認識して実行
// void receive_data(); // データの受信
// void send_data(cancel_flag&); // データの送信
// void output_info(); // 情報の出力
unsigned int id_of_pc(int); // PCから命令IDへの変換
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

/* インライン展開したいコンストラクタ */
inline Instruction::Instruction(){
    this->op = nop;
    this->rs1_v = 0;
    this->rs2_v = 0;
    this->pc = 0;
}
inline Configuration::ID_stage::ID_stage(){ // pcの初期値に注意
    this->pc[0] = -8;
    this->pc[1] = -4;
}
