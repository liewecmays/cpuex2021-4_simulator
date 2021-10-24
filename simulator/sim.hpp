#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <queue>
#include <boost/bimap/bimap.hpp>

/* 構造体などの定義 */
// 命令
struct Operation{
    int opcode;
    int funct;
    int rs1;
    int rs2;
    int rd;
    int imm;
};

// 整数と浮動小数点数の共用体 (todo: 仕様に合わせる)
union Int_float{
    int i;
    float f;
};

// boost::bimaps関連の略記
typedef boost::bimaps::bimap<std::string, unsigned int> bimap_t;
typedef bimap_t::value_type bimap_value_t;
typedef boost::bimaps::bimap<unsigned int, unsigned int> bimap_t2;
typedef bimap_t2::value_type bimap_value_t2;


/* extern宣言 */
extern std::vector<Operation> op_list;
extern std::vector<int> reg_list;
extern std::vector<float> reg_fp_list;
extern std::vector<Int_float> memory;

extern unsigned int pc;
extern int op_count;
extern int op_total;

extern int port;
extern std::queue<int> receive_buffer;

extern bool is_debug;
extern bool is_out;
extern std::stringstream output;

extern bimap_t bp_to_id;
extern bimap_t label_to_id;
extern bimap_t2 id_to_line;

extern bool loop_flag;

extern std::string head;
extern std::string error;
extern std::string info;
extern std::string data;
