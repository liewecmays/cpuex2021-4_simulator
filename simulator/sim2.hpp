#pragma once
#include <common.hpp>
#include <unit.hpp>
#include <fpu.hpp>
#include <string>
#include <vector>
#include <boost/bimap/bimap.hpp>
#include <exception>

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
extern Memory_with_cache memory;
extern Fpu fpu;
extern unsigned int code_size;
extern TransmissionQueue receive_buffer;
extern TransmissionQueue send_buffer;
extern bool is_debug;
extern bool is_quick;
extern bool is_ieee;
extern bimap_t bp_to_id;
extern bimap_t label_to_id;
extern bimap_t2 id_to_line;
extern unsigned long long* op_type_count;

/* プロトタイプ宣言 */
void simulate(); // シミュレーションの本体処理
bool exec_command(std::string); // デバッグモードのコマンドを認識して実行
// void output_info(); // 情報の出力
unsigned long long op_count(); // 実行命令の総数を返す
void exit_with_output(std::exception&); // 実行情報を表示したうえで異常終了
