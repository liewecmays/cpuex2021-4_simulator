#pragma once
#include <common.hpp>
#include <fpu.hpp>
#include <sim2.hpp>
#include <string>
#include <array>
#include <optional>
#include <nameof.hpp>

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
        // ハザードの種類
        enum class Hazard_type{
            No_hazard, // ハザードなし
            Trivial, // 1命令目が発行されないときの2命令目は自明に発行されない
            End, // 命令メモリの終わりに当たったとき
            // 同時発行される命令間
            Intra_RAW_rd_to_rs1, Intra_RAW_rd_to_rs2,
            Intra_WAW_rd_to_rd,
            Intra_control,
            Intra_structural_mem, Intra_structural_mfp, Intra_structural_pfp,
            // 同時発行ではない命令間
            Inter_RAW_ma_to_rs1, Inter_RAW_ma_to_rs2, Inter_RAW_mfp_to_rs1, Inter_RAW_mfp_to_rs2, Inter_RAW_pfp_to_rs1, Inter_RAW_pfp_to_rs2,
            Inter_WAW_ma_to_rd, Inter_WAW_mfp_to_rd, Inter_WAW_pfp_to_rd,
            Inter_structural_mem, Inter_structural_mfp,
            // 書き込みポート不十分
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
                            Idle, Store_data_mem, Load_data_mem_int, Load_data_mem_fp
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
                bool is_clear();
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
        int advance_clock(bool, std::string); // クロックを1つ分先に進める
        Hazard_type intra_hazard_detector(); // 同時発行される命令の間のハザード検出
        Hazard_type inter_hazard_detector(unsigned int); // 同時発行されない命令間のハザード検出
        Hazard_type iwp_hazard_detector(unsigned int); // 書き込みポート数が不十分な場合のハザード検出
        void wb_req(Instruction); // WBステージに命令を渡す
};
Configuration::Hazard_type operator||(Configuration::Hazard_type, Configuration::Hazard_type);

inline constexpr int sim_state_continue = -1;
inline constexpr int sim_state_end = -2;


/* class Instruction */
inline Instruction::Instruction(){
    this->op = Operation("nop");
    this->rs1_v = 0;
    this->rs2_v = 0;
    this->pc = 0;
}

/* class Configuration */
inline Configuration::ID_stage::ID_stage(){ // pcの初期値に注意
    this->pc[0] = -2;
    this->pc[1] = -1;
}

// クロックを1つ分先に進める
inline int Configuration::advance_clock(bool verbose, std::string bp){
    Configuration config_next = Configuration(); // *thisを現在の状態として、次の状態
    int res = sim_state_continue;

    config_next.clk = this->clk + 1;

    /* execution */
    // AL
    for(unsigned int i=0; i<2; ++i){
        this->EX.als[i].exec();
        if(!this->EX.als[i].inst.op.is_nop()){
            config_next.wb_req(this->EX.als[i].inst);
        }
    }

    // BR
    this->EX.br.exec();

    // MA
    if(!this->EX.ma.inst.op.is_nop()){
        if(this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle){
            switch(this->EX.ma.inst.op.type){
                case Otype::o_sw:
                case Otype::o_fsw:
                    if(this->EX.ma.available()){
                        this->EX.ma.exec();
                        config_next.EX.ma.state = this->EX.ma.state;
                    }else{
                        config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Store_data_mem;
                        config_next.EX.ma.inst = this->EX.ma.inst;
                        config_next.EX.ma.cycle_count = 1;
                    }
                    break;
                case Otype::o_lw:
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_int;
                    config_next.EX.ma.inst = this->EX.ma.inst;
                    config_next.EX.ma.cycle_count = 1;
                    break;
                case Otype::o_flw:
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_fp;
                    config_next.EX.ma.inst = this->EX.ma.inst;
                    config_next.EX.ma.cycle_count = 1;
                    break;
                // 以下は状態遷移しない命令
                case Otype::o_si:
                case Otype::o_std:
                    this->EX.ma.exec();
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Idle;
                    break;
                case Otype::o_lre:
                case Otype::o_lrd:
                case Otype::o_ltf:
                    this->EX.ma.exec();
                    config_next.wb_req(this->EX.ma.inst);
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Idle;
                    break;
                default: std::exit(EXIT_FAILURE);
            }
        }else{
            if(this->EX.ma.available()){
                this->EX.ma.exec();
                if(this->EX.ma.inst.op.type == Otype::o_lw || this->EX.ma.inst.op.type == Otype::o_flw){
                    config_next.wb_req(this->EX.ma.inst);
                }
                config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Idle;
            }else{
                config_next.EX.ma.state = this->EX.ma.state;
                config_next.EX.ma.inst = this->EX.ma.inst;
                config_next.EX.ma.cycle_count = this->EX.ma.cycle_count + 1;
            }
        }
    }

    // MA (hazard info)
    this->EX.ma.info.wb_addr = this->EX.ma.inst.op.rd;
    this->EX.ma.info.is_willing_but_not_ready_int =
        (this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle && this->EX.ma.inst.op.type == Otype::o_lw)
        || (this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_int && !this->EX.ma.available());
    this->EX.ma.info.is_willing_but_not_ready_fp =
        (this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle && this->EX.ma.inst.op.type == Otype::o_flw)
        || (this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_fp && !this->EX.ma.available());
    this->EX.ma.info.cannot_accept =
        this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle ?
            ((this->EX.ma.inst.op.type == Otype::o_sw || this->EX.ma.inst.op.type == Otype::o_fsw) && !this->EX.ma.available()) || this->EX.ma.inst.op.type == Otype::o_lw || this->EX.ma.inst.op.type == Otype::o_flw
            : !this->EX.ma.available();

    // mFP
    if(!this->EX.mfp.inst.op.is_nop()){
        if(this->EX.mfp.state == Configuration::EX_stage::EX_mfp::State_mfp::Waiting){
            if(this->EX.mfp.available()){
                this->EX.mfp.exec();
                config_next.wb_req(this->EX.mfp.inst);
                config_next.EX.mfp.state = this->EX.mfp.state;
            }else{
                config_next.EX.mfp.state = Configuration::EX_stage::EX_mfp::State_mfp::Processing;
                config_next.EX.mfp.inst = this->EX.mfp.inst;
                config_next.EX.mfp.cycle_count = 1;
            }
        }else{
            if(this->EX.mfp.available()){
                this->EX.mfp.exec();
                config_next.wb_req(this->EX.mfp.inst);
                config_next.EX.mfp.state = Configuration::EX_stage::EX_mfp::State_mfp::Waiting;
            }else{
                config_next.EX.mfp.state = this->EX.mfp.state;
                config_next.EX.mfp.inst = this->EX.mfp.inst;
                config_next.EX.mfp.cycle_count = this->EX.mfp.cycle_count + 1;
            }
        }
    }

    // mFP (hazard info)
    this->EX.mfp.info.wb_addr = this->EX.mfp.inst.op.rd;
    this->EX.mfp.info.is_willing_but_not_ready = (config_next.EX.mfp.state == Configuration::EX_stage::EX_mfp::State_mfp::Processing);
    this->EX.mfp.info.cannot_accept = (config_next.EX.mfp.state == Configuration::EX_stage::EX_mfp::State_mfp::Processing);

    // pFP
    for(unsigned int i=1; i<pipelined_fpu_stage_num; ++i){
        config_next.EX.pfp.inst[i] = this->EX.pfp.inst[i-1];
    }
    this->EX.pfp.exec();
    config_next.wb_req(this->EX.pfp.inst[pipelined_fpu_stage_num-1]);

    // pFP (hazard info)
    for(unsigned int i=0; i<pipelined_fpu_stage_num-1; ++i){
        this->EX.pfp.info.wb_addr[i] = this->EX.pfp.inst[i].op.rd;
        this->EX.pfp.info.wb_en[i] = this->EX.pfp.inst[i].op.use_pipelined_fpu();       
    }


    /* instruction fetch/decode */
    // dispatch?
    if(this->ID.op[0].is_nop() && this->ID.op[1].is_nop() && this->clk != 0){
        this->ID.hazard_type[0] = Configuration::Hazard_type::End;
        this->ID.hazard_type[1] = Configuration::Hazard_type::End;
    }else{
        this->ID.hazard_type[0] = this->inter_hazard_detector(0) || this->iwp_hazard_detector(0);
        if(this->ID.is_not_dispatched(0)){
            this->ID.hazard_type[1] = Configuration::Hazard_type::Trivial;
        }else{
            this->ID.hazard_type[1] = this->intra_hazard_detector() || this->inter_hazard_detector(1) || this->iwp_hazard_detector(1);
        }
    }

    // pc manager
    if(this->EX.br.branch_addr.has_value()){
        this->IF.pc[0] = this->EX.br.branch_addr.value();
        this->IF.pc[1] = this->EX.br.branch_addr.value() + 1;
    }else if(this->ID.is_not_dispatched(0)){
        this->IF.pc = this->ID.pc;
    }else if(this->ID.is_not_dispatched(1)){
        this->IF.pc[0] = this->ID.pc[0] + 1;
        this->IF.pc[1] = this->ID.pc[1] + 1;
    }else{
        this->IF.pc[0] = this->ID.pc[0] + 2;
        this->IF.pc[1] = this->ID.pc[1] + 2;
    }

    // instruction fetch
    config_next.ID.pc = this->IF.pc;
    config_next.ID.op[0] = op_list[this->IF.pc[0]];
    config_next.ID.op[1] = op_list[this->IF.pc[1]];

    // distribution + reg fetch
    for(unsigned int i=0; i<2; ++i){
        if(this->EX.br.branch_addr.has_value() || this->ID.is_not_dispatched(i)) continue; // rst from br/id
        switch(this->ID.op[i].type){
            // AL
            case Otype::o_add:
            case Otype::o_sub:
            case Otype::o_sll:
            case Otype::o_srl:
            case Otype::o_sra:
            case Otype::o_and:
            case Otype::o_addi:
            case Otype::o_slli:
            case Otype::o_srli:
            case Otype::o_srai:
            case Otype::o_andi:
            case Otype::o_lui:
                config_next.EX.als[i].inst.op = this->ID.op[i];
                config_next.EX.als[i].inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.als[i].inst.rs2_v = reg_int.read_32(this->ID.op[i].rs2);
                config_next.EX.als[i].inst.pc = this->ID.pc[i];
                break;
            case Otype::o_fmvfi:
                config_next.EX.als[i].inst.op = this->ID.op[i];
                config_next.EX.als[i].inst.rs1_v = reg_fp.read_32(this->ID.op[i].rs1);
                config_next.EX.als[i].inst.pc = this->ID.pc[i];
                break;
            // BR (conditional)
            case Otype::o_beq:
            case Otype::o_blt:
                config_next.EX.br.inst.op = this->ID.op[i];
                config_next.EX.br.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.br.inst.rs2_v = reg_int.read_32(this->ID.op[i].rs2);
                config_next.EX.br.inst.pc = this->ID.pc[i];
                break;
            case Otype::o_fbeq:
            case Otype::o_fblt:
                config_next.EX.br.inst.op = this->ID.op[i];
                config_next.EX.br.inst.rs1_v = reg_fp.read_32(this->ID.op[i].rs1);
                config_next.EX.br.inst.rs2_v = reg_fp.read_32(this->ID.op[i].rs2);
                config_next.EX.br.inst.pc = this->ID.pc[i];
                break;
            // BR (unconditional)
            case Otype::o_jal:
            case Otype::o_jalr:
                // ALとBRの両方にdistribute
                config_next.EX.als[i].inst.op = this->ID.op[i];
                config_next.EX.als[i].inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.als[i].inst.pc = this->ID.pc[i];
                config_next.EX.br.inst.op = this->ID.op[i];
                config_next.EX.br.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.br.inst.pc = this->ID.pc[i];
                break;
            // MA
            case Otype::o_sw:
            case Otype::o_si:
            case Otype::o_std:
            case Otype::o_lw:
            case Otype::o_lre:
            case Otype::o_lrd:
            case Otype::o_ltf:
            case Otype::o_flw:
                config_next.EX.ma.inst.op = this->ID.op[i];
                config_next.EX.ma.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.ma.inst.rs2_v = reg_int.read_32(this->ID.op[i].rs2);
                config_next.EX.ma.inst.pc = this->ID.pc[i];
                break;
            case Otype::o_fsw:
                config_next.EX.ma.inst.op = this->ID.op[i];
                config_next.EX.ma.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.ma.inst.rs2_v = reg_fp.read_32(this->ID.op[i].rs2);
                config_next.EX.ma.inst.pc = this->ID.pc[i];
                break;
            // mFP
            case Otype::o_fdiv:
            case Otype::o_fsqrt:
            case Otype::o_fcvtif:
            case Otype::o_fcvtfi:
            case Otype::o_fmvff:
                config_next.EX.mfp.inst.op = this->ID.op[i];
                config_next.EX.mfp.inst.rs1_v = reg_fp.read_32(this->ID.op[i].rs1);
                config_next.EX.mfp.inst.rs2_v = reg_fp.read_32(this->ID.op[i].rs2);
                config_next.EX.mfp.inst.pc = this->ID.pc[i];
                break;
            case Otype::o_fmvif:
                config_next.EX.mfp.inst.op = this->ID.op[i];
                config_next.EX.mfp.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.mfp.inst.pc = this->ID.pc[i];
                break;
            // pFP
            case Otype::o_fadd:
            case Otype::o_fsub:
            case Otype::o_fmul:
                config_next.EX.pfp.inst[0].op = this->ID.op[i];
                config_next.EX.pfp.inst[0].rs1_v = reg_fp.read_32(this->ID.op[i].rs1);
                config_next.EX.pfp.inst[0].rs2_v = reg_fp.read_32(this->ID.op[i].rs2);
                config_next.EX.pfp.inst[0].pc = this->ID.pc[i];
                break;
            case Otype::o_nop: break;
            default: std::exit(EXIT_FAILURE);
        }
    }

    /* 返り値の決定 */
    if(this->IF.pc[0] == static_cast<int>(code_size) && this->EX.is_clear()){ // 終了
        res = sim_state_end;
    }else if(is_debug && bp != "" && !this->EX.br.branch_addr.has_value()){
        if(bp == "__continue"){ // continue, 名前指定なし
            if(!this->ID.is_not_dispatched(0) && bp_to_id.right.find(this->ID.pc[0]) != bp_to_id.right.end()){
                res = this->ID.pc[0];
                verbose = true;
            }else if(!this->ID.is_not_dispatched(1) && bp_to_id.right.find(this->ID.pc[1]) != bp_to_id.right.end()){
                res = this->ID.pc[1];
                verbose = true;
            }
        }else{ // continue, 名前指定あり
            int bp_id = static_cast<int>(bp_to_id.left.at(bp));
            if(!this->ID.is_not_dispatched(0) && this->ID.pc[0] == bp_id){
                res = this->ID.pc[0];
                verbose = true;
            }else if(!this->ID.is_not_dispatched(1) && this->ID.pc[1] == bp_id){
                res = this->ID.pc[1];
                verbose = true;
            }
        }
    }

    /* print */
    if(verbose){
        std::cout << "clk: " << this->clk << std::endl;

        // IF
        std::cout << "\x1b[1m[IF]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            std::cout << (i==0 ? " " : "     ") << "if[" << i << "] : pc=" << this->IF.pc[i] << ((is_debug && this->IF.pc[i] < static_cast<int>(code_size)) ? (", line=" + std::to_string(id_to_line.left.at(this->IF.pc[i]))) : "") << std::endl;
        }

        // ID
        std::cout << "\x1b[1m[ID]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            std::cout << (i==0 ? " " : "     ") << "id[" << i << "] : " << this->ID.op[i].to_string() << " (pc=" << this->ID.pc[i] << ((is_debug && 0 <= this->ID.pc[i] && this->ID.pc[i] < static_cast<int>(code_size)) ? (", line=" + std::to_string(id_to_line.left.at(this->ID.pc[i]))) : "") << ")" << (this->ID.is_not_dispatched(i) ? ("\x1b[1m\x1b[31m -> not dispatched\x1b[0m [" + std::string(NAMEOF_ENUM(this->ID.hazard_type[i])) + "]") : "\x1b[1m -> dispatched\x1b[0m") << std::endl;
        }

        // EX
        std::cout << "\x1b[1m[EX]\x1b[0m";
        
        // EX_al
        for(unsigned int i=0; i<2; ++i){
            if(!this->EX.als[i].inst.op.is_nop()){
                std::cout << (i==0 ? " " : "     ") << "al" << i << "   : " << this->EX.als[i].inst.op.to_string() << " (pc=" << this->EX.als[i].inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.als[i].inst.pc))) : "") << ")" << std::endl;
            }else{
                std::cout << (i==0 ? " " : "     ") << "al" << i << "   :" << std::endl;
            }
        }

        // EX_br
        if(!this->EX.br.inst.op.is_nop()){
            std::cout << "     br    : " << this->EX.br.inst.op.to_string() << " (pc=" << this->EX.br.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.br.inst.pc))) : "") << ")" << (this->EX.br.branch_addr.has_value() ? "\x1b[1m -> taken\x1b[0m" : "\x1b[1m -> untaken\x1b[0m") << std::endl;
        }else{
            std::cout << "     br    :" << std::endl;
        }

        // EX_ma
        if(!this->EX.ma.inst.op.is_nop()){
            std::cout << "     ma    : " << this->EX.ma.inst.op.to_string() << " (pc=" << this->EX.ma.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.ma.inst.pc))) : "") << ") [state: " << NAMEOF_ENUM(this->EX.ma.state) << (this->EX.ma.state != Configuration::EX_stage::EX_ma::State_ma::Idle ? (", cycle: " + std::to_string(this->EX.ma.cycle_count)) : "") << "]" << (this->EX.ma.available() ? "\x1b[1m\x1b[32m -> available\x1b[0m" : "") << std::endl;
        }else{
            std::cout << "     ma    :" << std::endl;
        }

        // EX_mfp
        if(!this->EX.mfp.inst.op.is_nop()){
            std::cout << "     mfp   : " << this->EX.mfp.inst.op.to_string() << " (pc=" << this->EX.mfp.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.mfp.inst.pc))) : "") << ") [state: " << NAMEOF_ENUM(this->EX.mfp.state) << (this->EX.mfp.state != Configuration::EX_stage::EX_mfp::State_mfp::Waiting ? (", cycle: " + std::to_string(this->EX.mfp.cycle_count)) : "") << "]" << (this->EX.mfp.available() ? "\x1b[1m\x1b[32m -> available\x1b[0m" : "") << std::endl;
        }else{
            std::cout << "     mfp   :" << std::endl;
        }

        // EX_pfp
        for(unsigned int i=0; i<pipelined_fpu_stage_num; ++i){
            if(!this->EX.pfp.inst[i].op.is_nop()){
                std::cout << "     pfp[" << i << "]: " << this->EX.pfp.inst[i].op.to_string() << " (pc=" << this->EX.pfp.inst[i].pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.pfp.inst[i].pc))) : "") << ")" << std::endl;
            }else{
                std::cout << "     pfp[" << i << "]:" << std::endl;
            }
        }

        // WB
        std::cout << "\x1b[1m[WB]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            if(this->WB.inst_int[i].has_value()){
                std::cout << (i==0 ? " " : "     ") << "int[" << i << "]: " << this->WB.inst_int[i].value().op.to_string() << " (pc=" << this->WB.inst_int[i].value().pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->WB.inst_int[i].value().pc))) : "") << ")" << std::endl;
            }else{
                std::cout << (i==0 ? " " : "     ") << "int[" << i << "]:" << std::endl;
            }
        }
        for(unsigned int i=0; i<2; ++i){
            if(this->WB.inst_fp[i].has_value()){
                std::cout << "     fp[" << i << "] : " << this->WB.inst_fp[i].value().op.to_string() << " (pc=" << this->WB.inst_fp[i].value().pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->WB.inst_fp[i].value().pc))) : "") << ")" << std::endl;
            }else{
                std::cout << "     fp[" << i << "] :" << std::endl;
            }
        }
    }

    /* update */
    *this = config_next;

    return res;
}

// Hazard_type間のOR
inline Configuration::Hazard_type operator||(Configuration::Hazard_type t1, Configuration::Hazard_type t2){
    if(t1 == Configuration::Hazard_type::No_hazard){
        return t2;
    }else{
        return t1;
    }
}

// 同時発行される命令の間のハザード検出
inline Configuration::Hazard_type Configuration::intra_hazard_detector(){
    // RAW hazards
    if(
        ((this->ID.op[0].use_rd_int() && this->ID.op[1].use_rs1_int())
        || (this->ID.op[0].use_rd_fp() && this->ID.op[1].use_rs1_fp()))
        && this->ID.op[0].rd == this->ID.op[1].rs1
    ) return Configuration::Hazard_type::Intra_RAW_rd_to_rs1;
    if(
        ((this->ID.op[0].use_rd_int() && this->ID.op[1].use_rs2_int())
        || (this->ID.op[0].use_rd_fp() && this->ID.op[1].use_rs2_fp()))
        && this->ID.op[0].rd == this->ID.op[1].rs2
    ) return Configuration::Hazard_type::Intra_RAW_rd_to_rs2;

    // WAW hazards
    if(
        ((this->ID.op[0].use_rd_int() && this->ID.op[1].use_rd_int())
        || (this->ID.op[0].use_rd_fp() && this->ID.op[1].use_rd_fp()))
        && this->ID.op[0].rd == this->ID.op[1].rd
    ) return Configuration::Hazard_type::Intra_WAW_rd_to_rd;

    // control hazards
    if(
        this->ID.op[0].branch_conditionally_or_unconditionally()
    ) return Configuration::Hazard_type::Intra_control;

    // structural hazards
    if(
        this->ID.op[0].use_mem() && this->ID.op[1].use_mem()
    ) return Configuration::Hazard_type::Intra_structural_mem;
    if(
        this->ID.op[0].use_multicycle_fpu() && this->ID.op[1].use_multicycle_fpu()
    ) return Configuration::Hazard_type::Intra_structural_mfp;
    if(
        this->ID.op[0].use_pipelined_fpu() && this->ID.op[1].use_pipelined_fpu()
    ) return Configuration::Hazard_type::Intra_structural_pfp;

    // no hazard detected
    return Configuration::Hazard_type::No_hazard;
}

// 同時発行されない命令間のハザード検出
inline Configuration::Hazard_type Configuration::inter_hazard_detector(unsigned int i){ // i = 0,1
    // RAW hazards
    if(
        ((this->EX.ma.info.is_willing_but_not_ready_int && this->ID.op[i].use_rs1_int())
        || (this->EX.ma.info.is_willing_but_not_ready_fp && this->ID.op[i].use_rs1_fp()))
        && this->EX.ma.info.wb_addr == this->ID.op[i].rs1
    ) return Configuration::Hazard_type::Inter_RAW_ma_to_rs1;
    if(
        ((this->EX.ma.info.is_willing_but_not_ready_int && this->ID.op[i].use_rs2_int())
        || (this->EX.ma.info.is_willing_but_not_ready_fp && this->ID.op[i].use_rs2_fp()))
        && this->EX.ma.info.wb_addr == this->ID.op[i].rs2
    ) return Configuration::Hazard_type::Inter_RAW_ma_to_rs2;
    if(
        this->EX.mfp.info.is_willing_but_not_ready && this->ID.op[i].use_rs1_fp() && (this->EX.mfp.info.wb_addr == this->ID.op[i].rs1)
    ) return Configuration::Hazard_type::Inter_RAW_mfp_to_rs1;
    if(
        this->EX.mfp.info.is_willing_but_not_ready && this->ID.op[i].use_rs2_fp() && (this->EX.mfp.info.wb_addr == this->ID.op[i].rs2)
    ) return Configuration::Hazard_type::Inter_RAW_mfp_to_rs2;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.info.wb_en[j] && this->ID.op[i].use_rs1_fp() && (this->EX.pfp.info.wb_addr[j] == this->ID.op[i].rs1)){
            return Configuration::Hazard_type::Inter_RAW_pfp_to_rs1;
        }
    }
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.info.wb_en[j] && this->ID.op[i].use_rs2_fp() && (this->EX.pfp.info.wb_addr[j] == this->ID.op[i].rs2)){
            return Configuration::Hazard_type::Inter_RAW_pfp_to_rs2;
        }
    }

    // WAW hazards
    if(
        ((this->EX.ma.info.is_willing_but_not_ready_int && this->ID.op[i].use_rd_int())
        || (this->EX.ma.info.is_willing_but_not_ready_fp && this->ID.op[i].use_rd_fp()))
        && this->EX.ma.info.wb_addr == this->ID.op[i].rd
    ) return Configuration::Hazard_type::Inter_WAW_ma_to_rd;
    if(
        this->EX.mfp.info.is_willing_but_not_ready && this->ID.op[i].use_rd_fp() && (this->EX.mfp.info.wb_addr == this->ID.op[i].rd)
    ) return Configuration::Hazard_type::Inter_WAW_mfp_to_rd;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.info.wb_en[j] && this->ID.op[i].use_rd_fp() && (this->EX.pfp.info.wb_addr[j] == this->ID.op[i].rd)){
            return Configuration::Hazard_type::Inter_WAW_pfp_to_rd;
        }
    }

    // structural hazards
    if(
        this->EX.ma.info.cannot_accept && this->ID.op[i].use_mem()
    ) return Configuration::Hazard_type::Inter_structural_mem;
    if(
        this->EX.mfp.info.cannot_accept && this->ID.op[i].use_multicycle_fpu()
    ) return Configuration::Hazard_type::Inter_structural_mfp;

    // no hazard detected
    return Configuration::Hazard_type::No_hazard;
}

// 書き込みポート数が不十分な場合のハザード検出 (insufficient write port)
inline Configuration::Hazard_type Configuration::iwp_hazard_detector(unsigned int i){
    bool ma_wb_fp = this->EX.ma.info.is_willing_but_not_ready_fp;
    bool mfp_wb_fp = this->EX.mfp.info.is_willing_but_not_ready;
    bool pfp_wb_fp = false;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.info.wb_en[j]){
            pfp_wb_fp = true;
            break;
        }
    }

    // todo: COMPLEX_HAZARD_DETECTION
    if(i == 0){
        if(this->ID.op[0].use_rd_fp() && ((ma_wb_fp && mfp_wb_fp) || (mfp_wb_fp && pfp_wb_fp) || (pfp_wb_fp && ma_wb_fp))){
            return Configuration::Hazard_type::Insufficient_write_port;
        }else{
            return Configuration::Hazard_type::No_hazard;
        }
    }else if(i == 1){
        if(
            (this->ID.op[0].use_rd_int() && this->ID.op[1].use_rd_int() && this->EX.ma.info.is_willing_but_not_ready_int)
            || (this->ID.op[1].use_rd_fp() && ((ma_wb_fp && mfp_wb_fp) || (mfp_wb_fp && pfp_wb_fp) || (pfp_wb_fp && ma_wb_fp)))
            || (this->ID.op[0].use_rd_fp() && this->ID.op[1].use_rd_fp() && (ma_wb_fp || mfp_wb_fp || pfp_wb_fp))
        ){
            return Configuration::Hazard_type::Insufficient_write_port;
        }else{
            return Configuration::Hazard_type::No_hazard;
        }
    }else{
        std::exit(EXIT_FAILURE);
    }
}

// WBステージに命令を渡す
inline void Configuration::wb_req(Instruction inst){
    switch(inst.op.type){
        // int
        case Otype::o_add:
        case Otype::o_sub:
        case Otype::o_sll:
        case Otype::o_srl:
        case Otype::o_sra:
        case Otype::o_and:
        case Otype::o_addi:
        case Otype::o_slli:
        case Otype::o_srli:
        case Otype::o_srai:
        case Otype::o_andi:
        case Otype::o_lui:
        case Otype::o_fmvfi:
        case Otype::o_jal:
        case Otype::o_jalr:
        case Otype::o_lw:
        case Otype::o_lrd:
        case Otype::o_lre:
        case Otype::o_ltf:
            if(!this->WB.inst_int[0].has_value()){
                this->WB.inst_int[0] = inst;
            }else if(!this->WB.inst_int[1].has_value()){
                this->WB.inst_int[1] = inst;
            }else{
                exit_with_output("too many requests for WB(int) (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(inst.pc))) : "") + ")");
            }
            return;
        case Otype::o_fadd:
        case Otype::o_fsub:
        case Otype::o_fmul:
        case Otype::o_fdiv:
        case Otype::o_fsqrt:
        case Otype::o_fcvtif:
        case Otype::o_fcvtfi:
        case Otype::o_fmvff:
        case Otype::o_flw:
        case Otype::o_fmvif:
            if(!this->WB.inst_fp[0].has_value()){
                this->WB.inst_fp[0] = inst;
            }else if(!this->WB.inst_fp[1].has_value()){
                this->WB.inst_fp[1] = inst;
            }else{
                exit_with_output("too many requests for WB(int) (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(inst.pc))) : "") + ")");
            }
            return;
        case Otype::o_nop: return;
        default:
            exit_with_output("invalid request for WB (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(inst.pc))) : "") + ")");
    }
}

inline bool Configuration::ID_stage::is_not_dispatched(unsigned int i){
    return this->hazard_type[i] != Configuration::Hazard_type::No_hazard;
}

inline void Configuration::EX_stage::EX_al::exec(){
    switch(this->inst.op.type){
        // op
        case Otype::o_add:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i + this->inst.rs2_v.i);
            ++op_type_count[Otype::o_add];
            return;
        case Otype::o_sub:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i - this->inst.rs2_v.i);
            ++op_type_count[Otype::o_sub];
            return;
        case Otype::o_sll:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i << this->inst.rs2_v.i);
            ++op_type_count[Otype::o_sll];
            return;
        case Otype::o_srl:
            reg_int.write_int(this->inst.op.rd, static_cast<unsigned int>(this->inst.rs1_v.i) >> this->inst.rs2_v.i);
            ++op_type_count[Otype::o_srl];
            return;
        case Otype::o_sra:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i >> this->inst.rs2_v.i); // todo: 処理系依存
            ++op_type_count[Otype::o_sra];
            return;
        case Otype::o_and:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i & this->inst.rs2_v.i);
            ++op_type_count[Otype::o_and];
            return;
        // op_imm
        case Otype::o_addi:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i + this->inst.op.imm);
            ++op_type_count[Otype::o_addi];
            return;
        case Otype::o_slli:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i << this->inst.op.imm);
            ++op_type_count[Otype::o_slli];
            return;
        case Otype::o_srli:
            reg_int.write_int(this->inst.op.rd, static_cast<unsigned int>(this->inst.rs1_v.i) >> this->inst.op.imm);
            ++op_type_count[Otype::o_srli];
            return;
        case Otype::o_srai:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i >> this->inst.op.imm); // todo: 処理系依存
            ++op_type_count[Otype::o_srai];
            return;
        case Otype::o_andi:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i & this->inst.op.imm);
            ++op_type_count[Otype::o_andi];
            return;
        // lui
        case Otype::o_lui:
            reg_int.write_int(this->inst.op.rd, this->inst.op.imm << 12);
            ++op_type_count[Otype::o_lui];
            return;
        // ftoi
        case Otype::o_fmvfi:
            reg_int.write_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[Otype::o_fmvfi];
            return;
        // jalr (pass through)
        case Otype::o_jalr:
            reg_int.write_int(this->inst.op.rd, this->inst.pc + 1);
            return;
        // jal (pass through)
        case Otype::o_jal:
            reg_int.write_int(this->inst.op.rd, this->inst.pc + 1);
            return;
        case Otype::o_nop: return;
        default:
            exit_with_output("invalid operation for AL (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(this->inst.pc))) : "") + ")");
    }
}

inline void Configuration::EX_stage::EX_br::exec(){
    switch(this->inst.op.type){
        // branch
        case Otype::o_beq:
            if(this->inst.rs1_v.i == this->inst.rs2_v.i){
                this->branch_addr = this->inst.pc + this->inst.op.imm;
            }
            ++op_type_count[Otype::o_beq];
            return;
        case Otype::o_blt:
            if(this->inst.rs1_v.i < this->inst.rs2_v.i){
                this->branch_addr = this->inst.pc + this->inst.op.imm;
            }
            ++op_type_count[Otype::o_blt];
            return;
        // branch_fp
        case Otype::o_fbeq:
            if(this->inst.rs1_v.f == this->inst.rs2_v.f){
                this->branch_addr = this->inst.pc + this->inst.op.imm;
            }
            ++op_type_count[Otype::o_fbeq];
            return;
        case Otype::o_fblt:
            if(this->inst.rs1_v.f < this->inst.rs2_v.f){
                this->branch_addr = this->inst.pc + this->inst.op.imm;
            }
            ++op_type_count[Otype::o_fblt];
            return;
        // jalr
        case Otype::o_jalr:
            this->branch_addr = this->inst.rs1_v.ui;
            ++op_type_count[Otype::o_jalr];
            return;
        // jal
        case Otype::o_jal:
            this->branch_addr = this->inst.pc + this->inst.op.imm;
            ++op_type_count[Otype::o_jal];
            return;
        case Otype::o_nop: return;
        default:
            exit_with_output("invalid operation for BR (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(this->inst.pc))) : "") + ")");
    }
}

inline void Configuration::EX_stage::EX_ma::exec(){
    switch(this->inst.op.type){
        case Otype::o_sw:
            write_memory(this->inst.rs1_v.i + this->inst.op.imm, this->inst.rs2_v);
            ++op_type_count[Otype::o_sw];
            return;
        case Otype::o_si:
            op_list[this->inst.rs1_v.i + this->inst.op.imm] = Operation(this->inst.rs2_v.i);
            ++op_type_count[Otype::o_si];
            return;
        case Otype::o_std:
            send_buffer.push(inst.rs2_v);
            ++op_type_count[Otype::o_std];
            return;
        case Otype::o_fsw:
            write_memory(this->inst.rs1_v.i + this->inst.op.imm, this->inst.rs2_v);
            ++op_type_count[Otype::o_fsw];
            return;
        case Otype::o_lw:
            reg_int.write_32(this->inst.op.rd, read_memory(this->inst.rs1_v.i + this->inst.op.imm));
            ++op_type_count[Otype::o_lw];
            return;
        case Otype::o_lre:
            reg_int.write_int(this->inst.op.rd, receive_buffer.empty() ? 1 : 0);
            ++op_type_count[Otype::o_lre];
            return;
        case Otype::o_lrd:
            if(!receive_buffer.empty()){
                reg_int.write_32(this->inst.op.rd, receive_buffer.front());
                receive_buffer.pop();
            }else{
                exit_with_output("receive buffer is empty [lrd] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(this->inst.pc))) : "") + ")");
            }
            ++op_type_count[Otype::o_lrd];
            return;
        case Otype::o_ltf:
            reg_int.write_int(this->inst.op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
            ++op_type_count[Otype::o_ltf];
            return;
        case Otype::o_flw:
            reg_fp.write_32(this->inst.op.rd, read_memory(this->inst.rs1_v.i + this->inst.op.imm));
            ++op_type_count[Otype::o_flw];
            return;
        case Otype::o_nop: return;
        default:
            exit_with_output("invalid operation for MA (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(this->inst.pc))) : "") + ")");
    }
}

inline bool Configuration::EX_stage::EX_ma::available(){
    if(is_quick){
        return true;
    }else{
        return this->cycle_count == 2; // 仮の値
    }
}

inline void Configuration::EX_stage::EX_mfp::exec(){
    switch(this->inst.op.type){
        // op_fp
        case Otype::o_fdiv:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, this->inst.rs1_v.f / this->inst.rs2_v.f);
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.fdiv(this->inst.rs1_v, this->inst.rs2_v));
            }
            ++op_type_count[Otype::o_fdiv];
            return;
        case Otype::o_fsqrt:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, std::sqrt(this->inst.rs1_v.f));
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.fsqrt(this->inst.rs1_v));
            }
            ++op_type_count[Otype::o_fsqrt];
            return;
        case Otype::o_fcvtif:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, static_cast<float>(this->inst.rs1_v.i));
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.itof(this->inst.rs1_v));
            }
            ++op_type_count[Otype::o_fcvtif];
            return;
        case Otype::o_fcvtfi:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, static_cast<int>(std::nearbyint(this->inst.rs1_v.f)));
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.ftoi(this->inst.rs1_v));
            }
            ++op_type_count[Otype::o_fcvtfi];
            return;
        case Otype::o_fmvff:
            reg_fp.write_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[Otype::o_fmvff];
            return;
        // itof
        case Otype::o_fmvif:
            reg_fp.write_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[Otype::o_fmvif];
            return;
        case Otype::o_nop: return;
        default:
            exit_with_output("invalid operation for mFP (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(this->inst.pc))) : "") + ")");
    }
}

inline bool Configuration::EX_stage::EX_mfp::available(){
    if(is_quick){
        return true;
    }else{
        return this->cycle_count == 2; // 仮の値
    }
}

inline void Configuration::EX_stage::EX_pfp::exec(){
    Instruction inst = this->inst[pipelined_fpu_stage_num-1];
    switch(inst.op.type){
        case Otype::o_fadd:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f + inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fpu.fadd(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[Otype::o_fadd];
            return;
        case Otype::o_fsub:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f - inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fpu.fsub(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[Otype::o_fsub];
            return;
        case Otype::o_fmul:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f * inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fpu.fmul(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[Otype::o_fmul];
            return;
        case Otype::o_nop: return;
        default:
            exit_with_output("invalid operation for pFP (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(inst.pc))) : "") + ")");
    }
}

// EXステージに命令がないかどうかの判定
inline bool Configuration::EX_stage::is_clear(){
    bool pfp_clear = true;
    for(unsigned int i=0; i<pipelined_fpu_stage_num; ++i){
        if(!this->pfp.inst[i].op.is_nop()){
            pfp_clear = false;
            break;
        }
    }
    return (this->als[0].inst.op.is_nop() && this->als[1].inst.op.is_nop() && this->br.inst.op.is_nop() && this->ma.inst.op.is_nop() && this->mfp.inst.op.is_nop() && pfp_clear);
}