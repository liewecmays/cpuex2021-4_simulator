#pragma once
#include <params.hpp>
#include <common.hpp>
#include <unit.hpp>
#include <fpu.hpp>
#include <sim2.hpp>
#include <string>
#include <array>
#include <optional>
#include <exception>
#include <nameof.hpp>


// pc・命令
class Fetched_inst{
    public:
        Operation op;
        int pc;
        unsigned int pht_index;
        unsigned int pht_data;
        std::string to_string(){
            return std::to_string(pc) + ", " + op.to_string();
        }
};

// pc・命令・レジスタの値
class Instruction{
    public:
        Operation op;
        Bit32 rs1_v;
        Bit32 rs2_v;
        int pc;
        constexpr unsigned int ma_addr(){ return this->rs1_v.i + this->op.imm; }
};

// mod4で演算するだけのunsigned int
class m4ui{
    private:
        unsigned int i;
    public:
        constexpr m4ui(){ this->i = 0; }
        constexpr m4ui(const unsigned int x){ this->i = x % 4; };
        constexpr m4ui operator+=(const unsigned int x){
            this->i = (this->i + x) % 4;
            return *this;
        };
        constexpr m4ui operator+(const unsigned int x){
            return m4ui(*this) += x;
        };
        constexpr unsigned int val(){ return this->i; }
        constexpr unsigned int nxt(){ return (*this + 1).val(); }
        void print(){
            std::cout << this->i << std::endl;
        }
};

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
inline constexpr Hazard_type operator||(const Hazard_type t1, const Hazard_type t2){ // Hazard_type間のOR
    return (t1 == Hazard_type::No_hazard) ? t2 : t1;
}

// 各時点の状態
class Configuration{
    public:
        // instruction fetch
        class IF_stage{
            public:
                class IF_queue{
                    public:
                        m4ui head;
                        m4ui tail;
                        unsigned int num;
                        std::array<Fetched_inst, 4> array;
                };
            public:
                int fetch_addr = 0; // 先頭のみ保持
                IF_queue queue;
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
                        bool early_branch_taken;
                        bool actual_branch_taken;
                        unsigned int pht_index;
                        unsigned int pht_data;
                        std::optional<int> branch_addr;
                        void exec();
                };
                class EX_ma{
                    public:
                        std::array<Instruction, 3> inst;
                        void exec();
                };
                class EX_mfp{
                    public:
                        enum class State_mfp{
                            MFP_idle, MFP_busy, MFP_completed
                        };
                    public:
                        Instruction inst;
                        int remaining_cycle;
                        State_mfp state;
                        void exec();
                        constexpr bool available(){ return this->remaining_cycle == 0; }
                };
                class EX_pfp{
                    public:
                        std::array<Instruction, pipelined_fpu_stage_num> inst;
                        void exec();
                };
            public:
                std::array<EX_al, 2> als;
                EX_br br;
                EX_ma ma;
                EX_mfp mfp;
                EX_pfp pfp;
                constexpr bool is_clear();
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
        EX_stage EX;
        WB_stage WB;
        int advance_clock(bool, const std::string&); // クロックを1つ分先に進める
        constexpr Hazard_type intra_hazard_detector(const std::array<Fetched_inst, 2>&); // 同時発行される命令の間のハザード検出
        constexpr Hazard_type inter_hazard_detector(const Fetched_inst&); // 同時発行されない命令間のハザード検出
        constexpr Hazard_type iwp_hazard_detector(const std::array<Fetched_inst, 2>&, unsigned int); // 書き込みポート数が不十分な場合のハザード検出
        constexpr void wb_req_int(const Instruction&); // WBステージに命令を渡す
        constexpr void wb_req_fp(const Instruction&); // WBステージに命令を渡す
};


/* using宣言 */
using enum Otype;
using enum Stype;
using enum Hazard_type;
using enum Configuration::EX_stage::EX_mfp::State_mfp;


// クロックを1つ分先に進める
inline int Configuration::advance_clock(bool verbose, const std::string& bp){
    Configuration config_next = Configuration(); // *thisを現在の状態として、次の状態
    int res = sim_state_continue;

    config_next.clk = this->clk + 1;

    /* execution */
    // AL
    for(unsigned int i=0; i<2; ++i){
        this->EX.als[i].exec();
        config_next.wb_req_int(this->EX.als[i].inst);
    }

    // BR
    this->EX.br.exec();

    // MA
    /*
        note: 実際のコアの実装を単純化している、具体的には
        - 必ずヒットするものと仮定
        - 書き込み関係の状態管理をしない
        - req_rdy: fifoは一杯にならないと仮定して無視
        - rsp_en: 固定サイクル後に返ってくるものと仮定してmFPと同様に処理
    */
    // シミュレータの内部的な命令実行 (MA3)
    if(!this->EX.ma.inst[2].op.is_nop()){
        this->EX.ma.exec();
        if(this->EX.ma.inst[2].op.type == o_lre || this->EX.ma.inst[2].op.type == o_ltf || this->EX.ma.inst[2].op.type == o_lrd || this->EX.ma.inst[2].op.type == o_lw){
            config_next.wb_req_int(this->EX.ma.inst[2]);
        }else if(this->EX.ma.inst[2].op.type == o_si){
            config_next.wb_req_fp(this->EX.ma.inst[2]);
        }
    }
    
    // MA3 -> MA2
    // const bool ce = true;
    config_next.EX.ma.inst[1] = this->EX.ma.inst[0];
    config_next.EX.ma.inst[2] = this->EX.ma.inst[1];
    
    // mFP
    if(!this->EX.mfp.inst.op.is_nop()){
        switch(this->EX.mfp.state){
            case MFP_idle:
                if(this->EX.mfp.inst.op.is_nonzero_latency_mfp()){
                    config_next.EX.mfp.state = MFP_busy;
                    config_next.EX.mfp.inst = this->EX.mfp.inst;
                    switch(this->EX.mfp.inst.op.type){
                        case o_fdiv:
                            config_next.EX.mfp.remaining_cycle = 4; break;
                        case o_fsqrt:
                            config_next.EX.mfp.remaining_cycle = 1; break;
                        case o_fcvtif:
                        case o_fcvtfi:
                            config_next.EX.mfp.remaining_cycle = 0; break;
                        default: break;
                    }
                }else{
                    this->EX.mfp.exec();
                    config_next.wb_req_fp(this->EX.mfp.inst);
                    config_next.EX.mfp.state = this->EX.mfp.state;
                }
                break;
            case MFP_busy:
                if(this->EX.mfp.available()){
                    config_next.EX.mfp.state = MFP_completed;
                    config_next.EX.mfp.inst = this->EX.mfp.inst;
                }else{
                    config_next.EX.mfp.state = MFP_busy;
                    config_next.EX.mfp.remaining_cycle = this->EX.mfp.remaining_cycle - 1;
                    config_next.EX.mfp.inst = this->EX.mfp.inst;
                }
                break;
            case MFP_completed:
                this->EX.mfp.exec();
                config_next.wb_req_fp(this->EX.mfp.inst);
                break;
            default: break;
        }
    }

    // pFP
    for(unsigned int i=1; i<pipelined_fpu_stage_num; ++i){
        config_next.EX.pfp.inst[i] = this->EX.pfp.inst[i-1];
    }
    this->EX.pfp.exec();
    config_next.wb_req_fp(this->EX.pfp.inst[pipelined_fpu_stage_num-1]);

    /* instruction fetch + decode */
    std::array<Fetched_inst, 2> fetched_inst;
    fetched_inst[0] = this->IF.queue.array[this->IF.queue.head.val()];
    fetched_inst[1] = this->IF.queue.array[this->IF.queue.head.nxt()];

    // 命令発行の判定
    std::array<Hazard_type, 2> hazard_type;
    std::array<bool, 2> is_not_dispatched;
    if(fetched_inst[0].op.is_nop() && fetched_inst[1].op.is_nop() && this->clk != 0){
        hazard_type[0] = hazard_type[1] = End;
        is_not_dispatched[0] = is_not_dispatched[1] = true;
    }else{
        hazard_type[0] = this->inter_hazard_detector(fetched_inst[0]) || this->iwp_hazard_detector(fetched_inst, 0);
        if(hazard_type[0] != No_hazard){
            hazard_type[1] = Trivial;
            is_not_dispatched[0] = is_not_dispatched[1] = true;
        }else{
            hazard_type[1] = this->intra_hazard_detector(fetched_inst) || this->inter_hazard_detector(fetched_inst[1]) || this->iwp_hazard_detector(fetched_inst, 1);
            is_not_dispatched[0] = false;
            is_not_dispatched[1] = (hazard_type[1] != No_hazard);
        }
    }

    // ID段階での分岐の決定 (予測含む)
    bool id_branch_taken = fetched_inst[0].op.is_jal() || (fetched_inst[0].op.is_conditional() && !is_not_dispatched[0] && fetched_inst[0].pht_data >= 2); 
    
    // update `head`
    if(this->EX.br.branch_addr.has_value() || id_branch_taken){
        config_next.IF.queue.head = 0;
    }else{
        if(is_not_dispatched[0]){
            config_next.IF.queue.head = this->IF.queue.head;
        }else if(is_not_dispatched[1]){
            config_next.IF.queue.head = (this->IF.queue.num == 0) ? this->IF.queue.head : (this->IF.queue.head + 1);
        }else{
            config_next.IF.queue.head = (this->IF.queue.num == 0) ? this->IF.queue.head : (this->IF.queue.head + 2);
        }
    }

    // update `tail`
    if(this->EX.br.branch_addr.has_value() || id_branch_taken){
        config_next.IF.queue.head = 0;
    }else{
        switch(this->IF.queue.num){
            case 0:
            case 2:
                config_next.IF.queue.tail = (this->IF.queue.tail + 2);
                break;
            case 3:
                config_next.IF.queue.tail = (this->IF.queue.tail + 1);
                break;
            case 4:
                config_next.IF.queue.tail = this->IF.queue.tail;
                break;
            default: break;
        }
    }

    // update `num`
    if(this->EX.br.branch_addr.has_value() || id_branch_taken){
        config_next.IF.queue.num = 0;
    }else{
        if(is_not_dispatched[0]){
            switch(this->IF.queue.num){
                case 0: config_next.IF.queue.num = 2; break;
                case 2: config_next.IF.queue.num = 4; break;
                case 3: config_next.IF.queue.num = 4; break;
                case 4: config_next.IF.queue.num = 4; break;
                default: break;
            }
        }else if(is_not_dispatched[1]){
            switch(this->IF.queue.num){
                case 0: config_next.IF.queue.num = 2; break;
                case 2: config_next.IF.queue.num = 3; break;
                case 3: config_next.IF.queue.num = 3; break;
                case 4: config_next.IF.queue.num = 3; break;
                default: break;
            }
        }else{
            switch(this->IF.queue.num){
                case 0: config_next.IF.queue.num = 2; break;
                case 2: config_next.IF.queue.num = 2; break;
                case 3: config_next.IF.queue.num = 2; break;
                case 4: config_next.IF.queue.num = 2; break;
                default: break;
            }
        }
    }

    // update `fetch_addr`
    if(this->EX.br.branch_addr.has_value()){
        config_next.IF.fetch_addr = this->EX.br.branch_addr.value();
    }else if(id_branch_taken){
        config_next.IF.fetch_addr = fetched_inst[0].pc + fetched_inst[0].op.imm;
    }else{
        switch(this->IF.queue.num){
            case 0:
            case 2:
                config_next.IF.fetch_addr = this->IF.fetch_addr + 2;
                break;
            case 3:
                config_next.IF.fetch_addr = this->IF.fetch_addr + 1;
                break;
            case 4:
                config_next.IF.fetch_addr = this->IF.fetch_addr;
                break;
        }
    }

    // update queue
    if(this->EX.br.branch_addr.has_value() || id_branch_taken){
        // reset
    }else{
        config_next.IF.queue.array = this->IF.queue.array;

        Fetched_inst tmp;
        if(this->IF.queue.num < 4){
            tmp.pc = this->IF.fetch_addr;
            tmp.op = op_list[tmp.pc];
            tmp.pht_index = branch_predictor.pht_read_index(tmp.pc);
            tmp.pht_data = branch_predictor.pht_read_data(tmp.pht_index);
            config_next.IF.queue.array[this->IF.queue.tail.val()] = tmp;
        }
        if(this->IF.queue.num < 3){
            tmp.pc = this->IF.fetch_addr + 1;
            tmp.op = op_list[tmp.pc];
            tmp.pht_index = branch_predictor.pht_read_index(tmp.pc);
            tmp.pht_data = branch_predictor.pht_read_data(tmp.pht_index);
            config_next.IF.queue.array[this->IF.queue.tail.nxt()] = tmp;
        }
    }

    // 分岐予測の更新 (本来EXでやっているはずだったものを遅らせてここでやっている)
    if(this->EX.br.inst.op.is_conditional()) branch_predictor.update(this->EX.br.pht_index, this->EX.br.pht_data, this->EX.br.actual_branch_taken);

    // distribution + reg fetch
    for(unsigned int i=0; i<2; ++i){
        if(this->EX.br.branch_addr.has_value() || is_not_dispatched[i]) continue; // rst from br/id
        switch(fetched_inst[i].op.type){
            // AL
            case o_add:
            case o_sub:
            case o_sll:
            case o_srl:
            case o_sra:
            case o_and:
            case o_addi:
            case o_slli:
            case o_srli:
            case o_srai:
            case o_andi:
            case o_lui:
                config_next.EX.als[i].inst.op = fetched_inst[i].op;
                config_next.EX.als[i].inst.rs1_v = reg_int.read_32(fetched_inst[i].op.rs1);
                config_next.EX.als[i].inst.rs2_v = reg_int.read_32(fetched_inst[i].op.rs2);
                config_next.EX.als[i].inst.pc = fetched_inst[i].pc;
                break;
            case o_fmvfi:
                config_next.EX.als[i].inst.op = fetched_inst[i].op;
                config_next.EX.als[i].inst.rs1_v = reg_fp.read_32(fetched_inst[i].op.rs1);
                config_next.EX.als[i].inst.pc = fetched_inst[i].pc;
                break;
            // BR (conditional)
            case o_beq:
            case o_blt:
                config_next.EX.br.inst.op = fetched_inst[i].op;
                config_next.EX.br.inst.rs1_v = reg_int.read_32(fetched_inst[i].op.rs1);
                config_next.EX.br.inst.rs2_v = reg_int.read_32(fetched_inst[i].op.rs2);
                config_next.EX.br.inst.pc = fetched_inst[i].pc;
                config_next.EX.br.early_branch_taken = id_branch_taken;
                config_next.EX.br.pht_index = fetched_inst[i].pht_index;
                config_next.EX.br.pht_data = fetched_inst[i].pht_data;
                break;
            case o_fbeq:
            case o_fblt:
                config_next.EX.br.inst.op = fetched_inst[i].op;
                config_next.EX.br.inst.rs1_v = reg_fp.read_32(fetched_inst[i].op.rs1);
                config_next.EX.br.inst.rs2_v = reg_fp.read_32(fetched_inst[i].op.rs2);
                config_next.EX.br.inst.pc = fetched_inst[i].pc;
                config_next.EX.br.early_branch_taken = id_branch_taken;
                config_next.EX.br.pht_index = fetched_inst[i].pht_index;
                config_next.EX.br.pht_data = fetched_inst[i].pht_data;
                break;
            // BR (unconditional)
            case o_jal:
            case o_jalr:
                // ALとBRの両方にdistribute
                config_next.EX.als[i].inst.op = fetched_inst[i].op;
                config_next.EX.als[i].inst.rs1_v = reg_int.read_32(fetched_inst[i].op.rs1);
                config_next.EX.als[i].inst.pc = fetched_inst[i].pc;
                config_next.EX.br.inst.op = fetched_inst[i].op;
                config_next.EX.br.inst.rs1_v = reg_int.read_32(fetched_inst[i].op.rs1);
                config_next.EX.br.inst.pc = fetched_inst[i].pc;
                config_next.EX.br.early_branch_taken = id_branch_taken;
                config_next.EX.br.pht_index = fetched_inst[i].pht_index;
                config_next.EX.br.pht_data = fetched_inst[i].pht_data;
                break;
            // MA
            case o_sw:
            case o_si:
            case o_std:
            case o_lw:
            case o_lre:
            case o_lrd:
            case o_ltf:
            case o_flw:
                config_next.EX.ma.inst[0].op = fetched_inst[i].op;
                config_next.EX.ma.inst[0].rs1_v = reg_int.read_32(fetched_inst[i].op.rs1);
                config_next.EX.ma.inst[0].rs2_v = reg_int.read_32(fetched_inst[i].op.rs2);
                config_next.EX.ma.inst[0].pc = fetched_inst[i].pc;
                break;
            case o_fsw:
                config_next.EX.ma.inst[0].op = fetched_inst[i].op;
                config_next.EX.ma.inst[0].rs1_v = reg_int.read_32(fetched_inst[i].op.rs1);
                config_next.EX.ma.inst[0].rs2_v = reg_fp.read_32(fetched_inst[i].op.rs2);
                config_next.EX.ma.inst[0].pc = fetched_inst[i].pc;
                break;
            // mFP
            case o_fabs:
            case o_fneg:
            case o_fdiv:
            case o_fsqrt:
            case o_fcvtif:
            case o_fcvtfi:
            case o_fmvff:
                config_next.EX.mfp.inst.op = fetched_inst[i].op;
                config_next.EX.mfp.inst.rs1_v = reg_fp.read_32(fetched_inst[i].op.rs1);
                config_next.EX.mfp.inst.rs2_v = reg_fp.read_32(fetched_inst[i].op.rs2);
                config_next.EX.mfp.inst.pc = fetched_inst[i].pc;
                break;
            case o_fmvif:
                config_next.EX.mfp.inst.op = fetched_inst[i].op;
                config_next.EX.mfp.inst.rs1_v = reg_int.read_32(fetched_inst[i].op.rs1);
                config_next.EX.mfp.inst.pc = fetched_inst[i].pc;
                break;
            // pFP
            case o_fadd:
            case o_fsub:
            case o_fmul:
                config_next.EX.pfp.inst[0].op = fetched_inst[i].op;
                config_next.EX.pfp.inst[0].rs1_v = reg_fp.read_32(fetched_inst[i].op.rs1);
                config_next.EX.pfp.inst[0].rs2_v = reg_fp.read_32(fetched_inst[i].op.rs2);
                config_next.EX.pfp.inst[0].pc = fetched_inst[i].pc;
                break;
            case o_nop: break;
            default: std::exit(EXIT_FAILURE);
        }
    }

    /* 返り値の決定 */
    if(this->IF.fetch_addr >= static_cast<int>(code_size) && this->EX.is_clear()){ // 終了
        res = sim_state_end;
    }else if(is_debug && bp != "" && !this->EX.br.branch_addr.has_value()){
        if(bp == "__continue"){ // continue, 名前指定なし
            if(!is_not_dispatched[0] && bp_to_id.right.find(fetched_inst[0].pc) != bp_to_id.right.end()){
                res = fetched_inst[0].pc;
                verbose = true;
            }else if(!is_not_dispatched[1] && bp_to_id.right.find(fetched_inst[1].pc) != bp_to_id.right.end()){
                res = fetched_inst[1].pc;
                verbose = true;
            }
        }else{ // continue, 名前指定あり
            int bp_id = static_cast<int>(bp_to_id.left.at(bp));
            if(!is_not_dispatched[0] && fetched_inst[0].pc == bp_id){
                res = fetched_inst[0].pc;
                verbose = true;
            }else if(!is_not_dispatched[1] && fetched_inst[1].pc == bp_id){
                res = fetched_inst[1].pc;
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
            std::cout
            << (i==0 ? " " : "     ")
            << "if[" << i << "] : pc="
            << (this->IF.fetch_addr + i)
            << ((is_debug && (this->IF.fetch_addr + i) < static_cast<int>(code_size)) ? (", line=" + std::to_string(id_to_line.left.at(this->IF.fetch_addr + i))) : "")
            << std::endl;
        }

        // ID
        std::cout << "\x1b[1m[ID]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            std::cout
            << (i==0 ? " " : "     ")
            << "id[" << i << "] : "
            << fetched_inst[i].op.to_string() << " (pc=" << fetched_inst[i].pc
            << ((is_debug && 0 <= fetched_inst[i].pc && fetched_inst[i].pc < static_cast<int>(code_size)) ? (", line=" + std::to_string(id_to_line.left.at(fetched_inst[i].pc))) : "") << ")"
            << (is_not_dispatched[i] ? ("\x1b[1m\x1b[31m -> not dispatched\x1b[0m [" + std::string(NAMEOF_ENUM(hazard_type[i])) + "]") : "\x1b[1m -> dispatched\x1b[0m")
            << ((i == 0 && fetched_inst[i].op.is_conditional()) ? (id_branch_taken ? " [prediction: taken]" : " [prediction: untaken]") : "")
            << std::endl;
        }

        // EX
        std::cout << "\x1b[1m[EX]\x1b[0m";
        
        // EX_al
        for(unsigned int i=0; i<2; ++i){
            if(!this->EX.als[i].inst.op.is_nop()){
                std::cout
                << (i==0 ? " " : "     ")
                << "al" << i << "   : "
                << this->EX.als[i].inst.op.to_string()
                << " (pc=" << this->EX.als[i].inst.pc
                << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.als[i].inst.pc))) : "") << ")" << std::endl;
            }else{
                std::cout << (i==0 ? " " : "     ") << "al" << i << "   :" << std::endl;
            }
        }

        // EX_br
        if(!this->EX.br.inst.op.is_nop()){
            std::cout
            << "     br    : "
            << this->EX.br.inst.op.to_string()
            << " (pc=" << this->EX.br.inst.pc
            << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.br.inst.pc))) : "") << ")"
            << (this->EX.br.actual_branch_taken ? "\x1b[1m -> taken\x1b[0m" : "\x1b[1m -> untaken\x1b[0m")
            << (this->EX.br.branch_addr.has_value() ? " [miss]" : " [hit]") << std::endl;
        }else{
            std::cout << "     br    :" << std::endl;
        }

        // EX_ma
        for(int i=0; i<3; ++i){
            if(!this->EX.ma.inst[i].op.is_nop()){
                std::cout
                << "     ma[" << i << "] : "
                << this->EX.ma.inst[i].op.to_string()
                << " (pc=" << this->EX.ma.inst[0].pc
                << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.ma.inst[i].pc))) : "") << ")" << std::endl;
            }else{
                std::cout << "     ma[" << i << "] : " << std::endl;
            }
        }

        // EX_mfp
        if(!this->EX.mfp.inst.op.is_nop()){
            std::cout
            << "     mfp   : "
            << this->EX.mfp.inst.op.to_string()
            << " (pc=" << this->EX.mfp.inst.pc
            << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.mfp.inst.pc))) : "")
            << ") [state: " << NAMEOF_ENUM(this->EX.mfp.state) << (this->EX.mfp.state == MFP_busy ? (", remain: " + std::to_string(this->EX.mfp.remaining_cycle)) : "") << "]" << std::endl;
        }else{
            std::cout << "     mfp   :" << std::endl;
        }

        // EX_pfp
        for(unsigned int i=0; i<pipelined_fpu_stage_num; ++i){
            if(!this->EX.pfp.inst[i].op.is_nop()){
                std::cout
                << "     pfp[" << i << "]: "
                << this->EX.pfp.inst[i].op.to_string()
                << " (pc=" << this->EX.pfp.inst[i].pc
                << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(this->EX.pfp.inst[i].pc))) : "") << ")" << std::endl;
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

// 同時発行される命令の間のハザード検出
inline constexpr Hazard_type Configuration::intra_hazard_detector(const std::array<Fetched_inst, 2>& fetched_inst){
    // RAW hazards
    if(
        ((fetched_inst[0].op.use_rd_int() && fetched_inst[1].op.use_rs1_int())
        || (fetched_inst[0].op.use_rd_fp() && fetched_inst[1].op.use_rs1_fp()))
        && fetched_inst[0].op.rd == fetched_inst[1].op.rs1
    ) return Intra_RAW_rd_to_rs1;
    if(
        ((fetched_inst[0].op.use_rd_int() && fetched_inst[1].op.use_rs2_int())
        || (fetched_inst[0].op.use_rd_fp() && fetched_inst[1].op.use_rs2_fp()))
        && fetched_inst[0].op.rd == fetched_inst[1].op.rs2
    ) return Intra_RAW_rd_to_rs2;

    // WAW hazards
    if(
        ((fetched_inst[0].op.use_rd_int() && fetched_inst[1].op.use_rd_int())
        || (fetched_inst[0].op.use_rd_fp() && fetched_inst[1].op.use_rd_fp()))
        && fetched_inst[0].op.rd == fetched_inst[1].op.rd
    ) return Intra_WAW_rd_to_rd;

    // control hazards
    if(
        fetched_inst[0].op.branch_conditionally_or_unconditionally()
    ) return Intra_control;

    // structural hazards
    if(
        fetched_inst[0].op.use_mem() && fetched_inst[1].op.use_mem()
    ) return Intra_structural_mem;
    if(
        fetched_inst[0].op.use_multicycle_fpu() && fetched_inst[1].op.use_multicycle_fpu()
    ) return Intra_structural_mfp;
    if(
        fetched_inst[0].op.use_pipelined_fpu() && fetched_inst[1].op.use_pipelined_fpu()
    ) return Intra_structural_pfp;

    // no hazard detected
    return No_hazard;
}

// 同時発行されない命令間のハザード検出
inline constexpr Hazard_type Configuration::inter_hazard_detector(const Fetched_inst& inst){
    // RAW hazards
    if(
        (((this->EX.ma.inst[0].op.is_load() && inst.op.use_rs1_int())
        || (this->EX.ma.inst[0].op.is_load_fp() && inst.op.use_rs1_fp()))
        && this->EX.ma.inst[0].op.rd == inst.op.rs1) // ma1 -> rs1
        ||
        (((this->EX.ma.inst[1].op.is_load() && inst.op.use_rs1_int())
        || (this->EX.ma.inst[1].op.is_load_fp() && inst.op.use_rs1_fp()))
        && this->EX.ma.inst[1].op.rd == inst.op.rs1) // ma2 -> rs1
    ) return Inter_RAW_ma_to_rs1;
    if(
        (((this->EX.ma.inst[0].op.is_load() && inst.op.use_rs2_int())
        || (this->EX.ma.inst[0].op.is_load_fp() && inst.op.use_rs2_fp()))
        && this->EX.ma.inst[0].op.rd == inst.op.rs2) // ma1 -> rs2
        ||
        (((this->EX.ma.inst[1].op.is_load() && inst.op.use_rs2_int())
        || (this->EX.ma.inst[1].op.is_load_fp() && inst.op.use_rs2_fp()))
        && this->EX.ma.inst[1].op.rd == inst.op.rs2) // ma2 -> rs2
    ) return Inter_RAW_ma_to_rs2;

    bool mfp_is_willing_but_not_ready = (this->EX.mfp.state == MFP_idle && this->EX.mfp.inst.op.is_nonzero_latency_mfp()) || this->EX.mfp.state == MFP_busy;
    if(
        mfp_is_willing_but_not_ready && inst.op.use_rs1_fp() && (this->EX.mfp.inst.op.rd == inst.op.rs1)
    ) return Inter_RAW_mfp_to_rs1;
    if(
        mfp_is_willing_but_not_ready && inst.op.use_rs2_fp() && (this->EX.mfp.inst.op.rd == inst.op.rs2)
    ) return Inter_RAW_mfp_to_rs2;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.inst[j].op.use_pipelined_fpu() && inst.op.use_rs1_fp() && (this->EX.pfp.inst[j].op.rd == inst.op.rs1)){
            return Inter_RAW_pfp_to_rs1;
        }
    }
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.inst[j].op.use_pipelined_fpu() && inst.op.use_rs2_fp() && (this->EX.pfp.inst[j].op.rd == inst.op.rs2)){
            return Inter_RAW_pfp_to_rs2;
        }
    }

    // WAW hazards
    if(
        (((this->EX.ma.inst[0].op.is_load() && inst.op.use_rd_int())
        || (this->EX.ma.inst[0].op.is_load_fp() && inst.op.use_rd_fp()))
        && this->EX.ma.inst[0].op.rd == inst.op.rd) // ma1 -> rd
        ||
        (((this->EX.ma.inst[1].op.is_load() && inst.op.use_rd_int())
        || (this->EX.ma.inst[1].op.is_load_fp() && inst.op.use_rd_fp()))
        && this->EX.ma.inst[1].op.rd == inst.op.rd) // ma2 -> rd
    ) return Inter_WAW_ma_to_rd;

    if(
        mfp_is_willing_but_not_ready && inst.op.use_rd_fp() && (this->EX.mfp.inst.op.rd == inst.op.rd)
    ) return Inter_WAW_mfp_to_rd;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.inst[j].op.use_pipelined_fpu() && inst.op.use_rd_fp() && (this->EX.pfp.inst[j].op.rd == inst.op.rd)){
            return Inter_WAW_pfp_to_rd;
        }
    }

    // structural hazards
    // 実行中はキャッシュミスを無視
    // if(
    //     info.ma.cannot_accept && inst.op.use_mem()
    // ) return Inter_structural_mem;
    if(
        mfp_is_willing_but_not_ready // 本当は"cannot_accept"だが同じなので簡略化
        && inst.op.use_multicycle_fpu()
    ) return Inter_structural_mfp;

    // no hazard detected
    return No_hazard;
}

// 書き込みポート数が不十分な場合のハザード検出 (insufficient write port)
inline constexpr Hazard_type Configuration::iwp_hazard_detector(const std::array<Fetched_inst, 2>& fetched_inst, unsigned int i){
    bool ma_wb_int_instantly = this->EX.ma.inst[1].op.is_load();
    bool ma_wb_fp = this->EX.ma.inst[0].op.is_load_fp() || this->EX.ma.inst[1].op.is_load_fp();
    bool mfp_wb_fp = (this->EX.mfp.state == MFP_idle && this->EX.mfp.inst.op.is_nonzero_latency_mfp()) || this->EX.mfp.state == MFP_busy;
    bool pfp_wb_fp = false;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.inst[j].op.use_pipelined_fpu()){
            pfp_wb_fp = true;
            break;
        }
    }

    if(i == 0){
        if(
            (mfp_wb_fp && ((fetched_inst[0].op.is_load_fp() && pfp_wb_fp) || (fetched_inst[0].op.use_pipelined_fpu() && ma_wb_fp)))
            || (fetched_inst[0].op.use_multicycle_fpu() && pfp_wb_fp && ma_wb_fp)
        ){
            return Insufficient_write_port;
        }else{
            return No_hazard;
        }
    }else if(i == 1){
        if(
            (fetched_inst[0].op.use_alu() && fetched_inst[1].op.use_alu() && ma_wb_int_instantly)
            || (fetched_inst[1].op.use_rd_fp() && (fetched_inst[0].op.use_multicycle_fpu() || fetched_inst[1].op.use_multicycle_fpu() || mfp_wb_fp))
        ){
            return Insufficient_write_port;
        }else{
            return No_hazard;
        }
    }else{
        return No_hazard; // error
    }
}

// WBステージに命令を渡す
inline constexpr void Configuration::wb_req_int(const Instruction& inst){
    if(inst.op.is_nop()) return;
    if(!this->WB.inst_int[0].has_value()){
        this->WB.inst_int[0] = inst;
    }else if(!this->WB.inst_int[1].has_value()){
        this->WB.inst_int[1] = inst;
    }
}
inline constexpr void Configuration::wb_req_fp(const Instruction& inst){
    if(inst.op.is_nop()) return;
    if(!this->WB.inst_fp[0].has_value()){
        this->WB.inst_fp[0] = inst;
    }else if(!this->WB.inst_fp[1].has_value()){
        this->WB.inst_fp[1] = inst;
    }
}

inline void Configuration::EX_stage::EX_al::exec(){
    switch(this->inst.op.type){
        // op
        case o_add:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i + this->inst.rs2_v.i);
            ++op_type_count[o_add];
            return;
        case o_sub:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i - this->inst.rs2_v.i);
            ++op_type_count[o_sub];
            return;
        case o_sll:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i << this->inst.rs2_v.i);
            ++op_type_count[o_sll];
            return;
        case o_srl:
            reg_int.write_int(this->inst.op.rd, static_cast<unsigned int>(this->inst.rs1_v.i) >> this->inst.rs2_v.i);
            ++op_type_count[o_srl];
            return;
        case o_sra:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i >> this->inst.rs2_v.i); // todo: 処理系依存
            ++op_type_count[o_sra];
            return;
        case o_and:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i & this->inst.rs2_v.i);
            ++op_type_count[o_and];
            return;
        // op_imm
        case o_addi:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i + this->inst.op.imm);
            ++op_type_count[o_addi];
            return;
        case o_slli:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i << this->inst.op.imm);
            ++op_type_count[o_slli];
            return;
        case o_srli:
            reg_int.write_int(this->inst.op.rd, static_cast<unsigned int>(this->inst.rs1_v.i) >> this->inst.op.imm);
            ++op_type_count[o_srli];
            return;
        case o_srai:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i >> this->inst.op.imm); // todo: 処理系依存
            ++op_type_count[o_srai];
            return;
        case o_andi:
            reg_int.write_int(this->inst.op.rd, this->inst.rs1_v.i & this->inst.op.imm);
            ++op_type_count[o_andi];
            return;
        // lui
        case o_lui:
            reg_int.write_int(this->inst.op.rd, this->inst.op.imm << 12);
            ++op_type_count[o_lui];
            return;
        // ftoi
        case o_fmvfi:
            reg_int.write_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[o_fmvfi];
            return;
        // jalr (pass through)
        case o_jalr:
            reg_int.write_int(this->inst.op.rd, this->inst.pc + 1);
            return;
        // jal (pass through)
        case o_jal:
            reg_int.write_int(this->inst.op.rd, this->inst.pc + 1);
            return;
        default: return; // note: 仕様上はここにその他の命令が指定されることはないので、(インライン展開のために)例外を出していない。以下同様。
    }
}

inline void Configuration::EX_stage::EX_br::exec(){
    bool comp_res;
    int target = this->inst.pc + this->inst.op.imm;
    switch(this->inst.op.type){
        // branch
        case o_beq:
            comp_res = (this->inst.rs1_v.i == this->inst.rs2_v.i);
            ++op_type_count[o_beq];
            break;
        case o_blt:
            comp_res = (this->inst.rs1_v.i < this->inst.rs2_v.i);
            ++op_type_count[o_blt];
            break;
        // branch_fp
        case o_fbeq:
            comp_res = (this->inst.rs1_v.f == this->inst.rs2_v.f);
            ++op_type_count[o_fbeq];
            break;
        case o_fblt:
            comp_res = (this->inst.rs1_v.f < this->inst.rs2_v.f);
            ++op_type_count[o_fblt];
            break;
        // jalr
        case o_jalr:
            comp_res = true;
            target = this->inst.rs1_v.ui;
            ++op_type_count[o_jalr];
            break;
        // jal
        case o_jal:
            comp_res = true;
            ++op_type_count[o_jal];
            break;
        default: return;
    }

    this->actual_branch_taken = comp_res;

    // 外れた場合のみ分岐を有効化
    if(
        (this->inst.op.is_conditional() && (this->early_branch_taken != comp_res)) ||
        (this->inst.op.is_unconditional() && !this->early_branch_taken)
    ){
        this->branch_addr = (this->early_branch_taken ? this->inst.pc + 1 : target);
    }
}

inline void Configuration::EX_stage::EX_ma::exec(){
    switch(this->inst[2].op.type){
        case o_sw:
            memory.write(this->inst[2].ma_addr(), this->inst[2].rs2_v);
            ++op_type_count[o_sw];
            return;
        case o_si:
            op_list[this->inst[2].rs1_v.i + this->inst[2].op.imm] = Operation(this->inst[2].rs2_v.i);
            ++op_type_count[o_si];
            return;
        case o_std:
            send_buffer.push(this->inst[2].rs2_v);
            ++op_type_count[o_std];
            return;
        case o_fsw:
            memory.write(this->inst[2].ma_addr(), this->inst[2].rs2_v);
            ++op_type_count[o_fsw];
            return;
        case o_lw:
            reg_int.write_32(this->inst[2].op.rd, memory.read(this->inst[2].ma_addr()));
            ++op_type_count[o_lw];
            return;
        case o_lre:
            reg_int.write_int(this->inst[2].op.rd, receive_buffer.empty() ? 1 : 0);
            ++op_type_count[o_lre];
            return;
        case o_lrd:
            if(!receive_buffer.empty()){
                reg_int.write_32(this->inst[2].op.rd, receive_buffer.pop());
            }else{
                throw std::runtime_error("receive buffer is empty [lrd] (at pc " + std::to_string(this->inst[2].pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(this->inst[2].pc))) : "") + ")");
            }
            ++op_type_count[o_lrd];
            return;
        case o_ltf:
            reg_int.write_int(this->inst[2].op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
            ++op_type_count[o_ltf];
            return;
        case o_flw:
            reg_fp.write_32(this->inst[2].op.rd, memory.read(this->inst[2].ma_addr()));
            ++op_type_count[o_flw];
            return;
        default: return;
    }
}

inline void Configuration::EX_stage::EX_mfp::exec(){
    switch(this->inst.op.type){
        // op_fp
        case o_fabs:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, std::abs(reg_fp.read_float(this->inst.op.rs1)));
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.fabs(reg_fp.read_32(this->inst.op.rs1)));
            }
            ++op_type_count[o_fabs];
            return;
        case o_fneg:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, - reg_fp.read_float(this->inst.op.rs1));
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.fneg(reg_fp.read_32(this->inst.op.rs1)));
            }
            ++op_type_count[o_fneg];
            return;
        case o_fdiv:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, this->inst.rs1_v.f / this->inst.rs2_v.f);
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.fdiv(this->inst.rs1_v, this->inst.rs2_v));
            }
            ++op_type_count[o_fdiv];
            return;
        case o_fsqrt:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, std::sqrt(this->inst.rs1_v.f));
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.fsqrt(this->inst.rs1_v));
            }
            ++op_type_count[o_fsqrt];
            return;
        case o_fcvtif:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, static_cast<float>(this->inst.rs1_v.i));
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.itof(this->inst.rs1_v));
            }
            ++op_type_count[o_fcvtif];
            return;
        case o_fcvtfi:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, static_cast<int>(std::nearbyint(this->inst.rs1_v.f)));
            }else{
                reg_fp.write_32(this->inst.op.rd, fpu.ftoi(this->inst.rs1_v));
            }
            ++op_type_count[o_fcvtfi];
            return;
        case o_fmvff:
            reg_fp.write_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[o_fmvff];
            return;
        // itof
        case o_fmvif:
            reg_fp.write_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[o_fmvif];
            return;
        default: return;
    }
}

inline void Configuration::EX_stage::EX_pfp::exec(){
    Instruction inst = this->inst[pipelined_fpu_stage_num-1];
    switch(inst.op.type){
        case o_fadd:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f + inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fpu.fadd(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fadd];
            return;
        case o_fsub:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f - inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fpu.fsub(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fsub];
            return;
        case o_fmul:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f * inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fpu.fmul(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fmul];
            return;
        default: return;
    }
}

// EXステージに命令がないかどうかの判定
inline constexpr bool Configuration::EX_stage::is_clear(){
    bool ma_clear = this->ma.inst[0].op.is_nop() && this->ma.inst[1].op.is_nop() && this->ma.inst[2].op.is_nop();
    bool pfp_clear = true;
    for(unsigned int i=0; i<pipelined_fpu_stage_num; ++i){
        if(!this->pfp.inst[i].op.is_nop()){
            pfp_clear = false;
            break;
        }
    }
    return (this->als[0].inst.op.is_nop() && this->als[1].inst.op.is_nop() && this->br.inst.op.is_nop() && ma_clear && this->mfp.inst.op.is_nop() && pfp_clear);
}
