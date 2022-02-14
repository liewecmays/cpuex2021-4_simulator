#pragma once
#include <common.hpp>
#include <unit.hpp>
#include <string>
#include <queue>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/bimap/bimap.hpp>
#include <exception>

/* typedef宣言 */
// boost::bimaps関連の略記
typedef boost::bimaps::bimap<std::string, unsigned int> bimap_t;
typedef bimap_t::value_type bimap_value_t;
typedef boost::bimaps::bimap<unsigned int, int> bimap_t2;
typedef bimap_t2::value_type bimap_value_t2;

/* extern宣言 */
extern int port;
extern TransmissionQueue receive_buffer;
extern TransmissionQueue send_buffer;
extern bool is_raytracing;

/* プロトタイプ宣言 */
void simulate(); // シミュレーションの本体処理
bool exec_command(std::string); // デバッグモードのコマンドを認識して実行
void output_info(); // 情報の出力
void exec_op(); // 命令を実行し、PCを変化させる
Bit32 read_memory(int); // メモリ読み出し(class Memoryのラッパー関数)
void write_memory(int, const Bit32&); // メモリ書き込み(class Memoryのラッパー関数)
bool constexpr is_end(const Operation&); // 終了時の命令かどうかを判定
void exit_with_output(std::exception&); // 実効情報を表示したうえで異常終了
