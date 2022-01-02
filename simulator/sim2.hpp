#pragma once
#include "common.hpp"
#include <string>
#include <array>
#include <optional>
#include <queue>
#include <boost/bimap/bimap.hpp>

/* typedef宣言 */
// boost::bimaps関連の略記
typedef boost::bimaps::bimap<std::string, unsigned int> bimap_t;
typedef bimap_t::value_type bimap_value_t;
typedef boost::bimaps::bimap<unsigned int, int> bimap_t2;
typedef bimap_t2::value_type bimap_value_t2;

/* クラスの定義 */
// pcと命令(レジスタや即値の値を本当に持っている)
class Instruction{
    public:
        Operation op;
        Bit32 rs1_v;
        Bit32 rs2_v;
        int pc;
        Instruction();
};

// 各時点の状態
class Configuration{
    public:
        // hazard type
        enum class Hazard_type{
            No_hazard, Trivial,
            // intra
            Intra_RAW_rd_to_rs1, Intra_RAW_rd_to_rs2,
            Intra_WAW_rd_to_rd,
            Intra_control,
            Intra_structural_mem, Intra_structural_mfp, Intra_structural_pfp,
            // inter
            Inter_RAW_ma_to_rs1, Inter_RAW_ma_to_rs2, Inter_RAW_mfp_to_rs1, Inter_RAW_mfp_to_rs2, Inter_RAW_pfp_to_rs1, Inter_RAW_pfp_to_rs2,
            Inter_WAW_ma_to_rd, Inter_WAW_mfp_to_rd, Inter_WAW_pfp_to_rd,
            Inter_structural_mem, Inter_structural_mfp,
            // iwp
            Insufficient_write_port
        };

        // instruction fetch
        class IF_stage{
            public:
                std::array<int, 2> pc;
        };

        // instruction decode
        class ID_stage{
            public:
                std::array<int, 2> pc;
                std::array<Operation, 2> op;
                std::array<Hazard_type, 2> hazard_type;
                ID_stage();
                bool is_not_dispatched(unsigned int);
        };

        // execution
        class EX_stage{
            public:
                class EX_al{
                    public:
                        Instruction inst;
                        void exec();
                };
                class EX_br{
                    public:
                        Instruction inst;
                        std::optional<unsigned int> branch_addr;
                        void exec();
                };
                class EX_ma{
                    public:
                        enum class State_ma{
                            Idle, Recv_uart, Store_data_mem, Load_data_mem_int, Load_data_mem_fp
                        };
                        class Hazard_info_ma{
                            public:
                                unsigned int wb_addr;
                                bool is_willing_but_not_ready_int;
                                bool is_willing_but_not_ready_fp;
                                bool cannot_accept;
                        };
                    public:
                        Instruction inst;
                        unsigned int cycle_count;
                        State_ma state;
                        Hazard_info_ma info;
                        void exec();
                        bool available();
                };
                class EX_mfp{
                    public:
                        enum class State_mfp{
                            Waiting, Processing
                        };
                        class Hazard_info_mfp{
                            public:
                                unsigned int wb_addr;
                                bool is_willing_but_not_ready;
                                bool cannot_accept;
                        };
                    public:
                        Instruction inst;
                        unsigned int cycle_count;
                        State_mfp state;
                        Hazard_info_mfp info;
                        void exec();
                        bool available();
                };
                class EX_pfp{
                    public:
                        class Hazard_info_pfp{
                            public:
                                std::array<unsigned int, pipelined_fpu_stage_num-1> wb_addr;
                                std::array<bool, pipelined_fpu_stage_num-1> wb_en;
                        };
                    public:
                        std::array<Instruction, pipelined_fpu_stage_num> inst;
                        Hazard_info_pfp info;
                        void exec();
                };
            public:
                std::array<EX_al, 2> als;
                EX_br br;
                EX_ma ma;
                EX_mfp mfp;
                EX_pfp pfp;
        };

        // write back
        class WB_stage{
            public:
                std::array<std::optional<Instruction>, 2> inst_int;
                std::array<std::optional<Instruction>, 2> inst_fp;
        };

    public:
        unsigned long long clk = 0;
        IF_stage IF;
        ID_stage ID;
        EX_stage EX;
        WB_stage WB;
        Hazard_type intra_hazard_detector(); // 同時発行される命令の間のハザード検出
        Hazard_type inter_hazard_detector(unsigned int); // 同時発行されない命令間のハザード検出
        Hazard_type iwp_hazard_detector(unsigned int); // 書き込みポート数が不十分な場合のハザード検出
        void wb_req(Instruction); // WBステージに命令を渡す
};


/* プロトタイプ宣言 */
void simulate(); // シミュレーションの本体処理
bool exec_command(std::string); // デバッグモードのコマンドを認識して実行
// void receive_data(); // データの受信
// void send_data(cancel_flag&); // データの送信
// void output_info(); // 情報の出力

void advance_clock(bool); // クロックを1つ分先に進める
Configuration::Hazard_type operator||(Configuration::Hazard_type, Configuration::Hazard_type);

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
