#pragma once
#include <string>

/* 定数 */
#define THREAD_NUM 100

/* プロトタイプ宣言 */
void server(); // コマンド入力をもとにデータを送信
bool exec_command(std::string cmd); // コマンドを読み、実行
void receive(); // データの受信
