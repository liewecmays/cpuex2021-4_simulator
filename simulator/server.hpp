#pragma once
#include <string>

/* プロトタイプ宣言 */
void server(); // コマンド入力をもとにデータを送信
bool exec_command(std::string cmd); // コマンドを読み、実行
void receive(); // データの受信
