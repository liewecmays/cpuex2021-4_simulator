#include <config.hpp>
#include <sim2.hpp>
#include <common.hpp>
#include <util.hpp>
#include <fpu.hpp>
#include <array>
#include <optional>
#include <string>
#include <iostream>
#include <boost/bimap/bimap.hpp>
#include "nameof.hpp"

using ::Otype;

// クロックを1つ分先に進める
int Configuration::advance_clock(bool verbose, std::string bp){
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
                case o_sw:
                case o_fsw:
                    if(this->EX.ma.available()){
                        this->EX.ma.exec();
                        config_next.EX.ma.state = this->EX.ma.state;
                    }else{
                        config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Store_data_mem;
                        config_next.EX.ma.inst = this->EX.ma.inst;
                        config_next.EX.ma.cycle_count = 1;
                    }
                    break;
                case o_lw:
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_int;
                    config_next.EX.ma.inst = this->EX.ma.inst;
                    config_next.EX.ma.cycle_count = 1;
                    break;
                case o_flw:
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_fp;
                    config_next.EX.ma.inst = this->EX.ma.inst;
                    config_next.EX.ma.cycle_count = 1;
                    break;
                // 以下は状態遷移しない命令
                case o_si:
                case o_std:
                    this->EX.ma.exec();
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Idle;
                    break;
                case o_lre:
                case o_lrd:
                case o_ltf:
                    this->EX.ma.exec();
                    config_next.wb_req(this->EX.ma.inst);
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Idle;
                    break;
                default: std::exit(EXIT_FAILURE);
            }
        }else{
            if(this->EX.ma.available()){
                this->EX.ma.exec();
                if(this->EX.ma.inst.op.type == o_lw || this->EX.ma.inst.op.type == o_flw){
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
        (this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle && this->EX.ma.inst.op.type == o_lw)
        || (this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_int && !this->EX.ma.available());
    this->EX.ma.info.is_willing_but_not_ready_fp =
        (this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle && this->EX.ma.inst.op.type == o_flw)
        || (this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_fp && !this->EX.ma.available());
    this->EX.ma.info.cannot_accept =
        this->EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle ?
            ((this->EX.ma.inst.op.type == o_sw || this->EX.ma.inst.op.type == o_fsw) && !this->EX.ma.available()) || this->EX.ma.inst.op.type == o_lw || this->EX.ma.inst.op.type == o_flw
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
        this->IF.pc[1] = this->EX.br.branch_addr.value() + 4;
    }else if(this->ID.is_not_dispatched(0)){
        this->IF.pc = this->ID.pc;
    }else if(this->ID.is_not_dispatched(1)){
        this->IF.pc[0] = this->ID.pc[0] + 4;
        this->IF.pc[1] = this->ID.pc[1] + 4;
    }else{
        this->IF.pc[0] = this->ID.pc[0] + 8;
        this->IF.pc[1] = this->ID.pc[1] + 8;
    }

    // instruction fetch
    config_next.ID.pc = this->IF.pc;
    config_next.ID.op[0] = op_list[id_of_pc(this->IF.pc[0])];
    config_next.ID.op[1] = op_list[id_of_pc(this->IF.pc[1])];

    // distribution + reg fetch
    for(unsigned int i=0; i<2; ++i){
        if(this->EX.br.branch_addr.has_value() || this->ID.is_not_dispatched(i)) continue; // rst from br/id
        switch(this->ID.op[i].type){
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
                config_next.EX.als[i].inst.op = this->ID.op[i];
                config_next.EX.als[i].inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.als[i].inst.rs2_v = reg_int.read_32(this->ID.op[i].rs2);
                config_next.EX.als[i].inst.pc = this->ID.pc[i];
                break;
            case o_fmvfi:
                config_next.EX.als[i].inst.op = this->ID.op[i];
                config_next.EX.als[i].inst.rs1_v = reg_fp.read_32(this->ID.op[i].rs1);
                config_next.EX.als[i].inst.pc = this->ID.pc[i];
                break;
            // BR (conditional)
            case o_beq:
            case o_blt:
                config_next.EX.br.inst.op = this->ID.op[i];
                config_next.EX.br.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.br.inst.rs2_v = reg_int.read_32(this->ID.op[i].rs2);
                config_next.EX.br.inst.pc = this->ID.pc[i];
                break;
            case o_fbeq:
            case o_fblt:
                config_next.EX.br.inst.op = this->ID.op[i];
                config_next.EX.br.inst.rs1_v = reg_fp.read_32(this->ID.op[i].rs1);
                config_next.EX.br.inst.rs2_v = reg_fp.read_32(this->ID.op[i].rs2);
                config_next.EX.br.inst.pc = this->ID.pc[i];
                break;
            // BR (unconditional)
            case o_jal:
            case o_jalr:
                // ALとBRの両方にdistribute
                config_next.EX.als[i].inst.op = this->ID.op[i];
                config_next.EX.als[i].inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.als[i].inst.pc = this->ID.pc[i];
                config_next.EX.br.inst.op = this->ID.op[i];
                config_next.EX.br.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.br.inst.pc = this->ID.pc[i];
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
                config_next.EX.ma.inst.op = this->ID.op[i];
                config_next.EX.ma.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.ma.inst.rs2_v = reg_int.read_32(this->ID.op[i].rs2);
                config_next.EX.ma.inst.pc = this->ID.pc[i];
                break;
            case o_fsw:
                config_next.EX.ma.inst.op = this->ID.op[i];
                config_next.EX.ma.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.ma.inst.rs2_v = reg_fp.read_32(this->ID.op[i].rs2);
                config_next.EX.ma.inst.pc = this->ID.pc[i];
                break;
            // mFP
            case o_fdiv:
            case o_fsqrt:
            case o_fcvtif:
            case o_fcvtfi:
            case o_fmvff:
                config_next.EX.mfp.inst.op = this->ID.op[i];
                config_next.EX.mfp.inst.rs1_v = reg_fp.read_32(this->ID.op[i].rs1);
                config_next.EX.mfp.inst.rs2_v = reg_fp.read_32(this->ID.op[i].rs2);
                config_next.EX.mfp.inst.pc = this->ID.pc[i];
                break;
            case o_fmvif:
                config_next.EX.mfp.inst.op = this->ID.op[i];
                config_next.EX.mfp.inst.rs1_v = reg_int.read_32(this->ID.op[i].rs1);
                config_next.EX.mfp.inst.pc = this->ID.pc[i];
                break;
            // pFP
            case o_fadd:
            case o_fsub:
            case o_fmul:
                config_next.EX.pfp.inst[0].op = this->ID.op[i];
                config_next.EX.pfp.inst[0].rs1_v = reg_fp.read_32(this->ID.op[i].rs1);
                config_next.EX.pfp.inst[0].rs2_v = reg_fp.read_32(this->ID.op[i].rs2);
                config_next.EX.pfp.inst[0].pc = this->ID.pc[i];
                break;
            case o_nop: break;
            default: std::exit(EXIT_FAILURE);
        }
    }

    /* 返り値の決定 */
    if(this->IF.pc[0] == static_cast<int>(code_size*4) && this->EX.is_clear()){ // 終了
        res = sim_state_end;
    }else if(is_debug && bp != "" && !this->EX.br.branch_addr.has_value()){
        if(bp == "__continue"){ // continue, 名前指定なし
            if(!this->ID.is_not_dispatched(0) && bp_to_id.right.find(this->ID.pc[0] / 4) != bp_to_id.right.end()){
                res = this->ID.pc[0];
                verbose = true;
            }else if(!this->ID.is_not_dispatched(1) && bp_to_id.right.find(this->ID.pc[1] / 4) != bp_to_id.right.end()){
                res = this->ID.pc[1];
                verbose = true;
            }
        }else{ // continue, 名前指定あり
            int bp_id = static_cast<int>(bp_to_id.left.at(bp));
            if(!this->ID.is_not_dispatched(0) && this->ID.pc[0] / 4 == bp_id){
                res = this->ID.pc[0];
                verbose = true;
            }else if(!this->ID.is_not_dispatched(1) && this->ID.pc[1] / 4 == bp_id){
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
            std::cout << (i==0 ? " " : "     ") << "if[" << i << "] : pc=" << this->IF.pc[i] << ((is_debug && this->IF.pc[i] < static_cast<int>(code_size*4)) ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->IF.pc[i])))) : "") << std::endl;
        }

        // ID
        std::cout << "\x1b[1m[ID]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            std::cout << (i==0 ? " " : "     ") << "id[" << i << "] : " << this->ID.op[i].to_string() << " (pc=" << this->ID.pc[i] << ((is_debug && 0 <= this->ID.pc[i] && this->ID.pc[i] < static_cast<int>(code_size*4)) ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->ID.pc[i])))) : "") << ")" << (this->ID.is_not_dispatched(i) ? ("\x1b[1m\x1b[31m -> not dispatched\x1b[0m [" + std::string(NAMEOF_ENUM(this->ID.hazard_type[i])) + "]") : "\x1b[1m -> dispatched\x1b[0m") << std::endl;
        }

        // EX
        std::cout << "\x1b[1m[EX]\x1b[0m";
        
        // EX_al
        for(unsigned int i=0; i<2; ++i){
            if(!this->EX.als[i].inst.op.is_nop()){
                std::cout << (i==0 ? " " : "     ") << "al" << i << "   : " << this->EX.als[i].inst.op.to_string() << " (pc=" << this->EX.als[i].inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->EX.als[i].inst.pc)))) : "") << ")" << std::endl;
            }else{
                std::cout << (i==0 ? " " : "     ") << "al" << i << "   :" << std::endl;
            }
        }

        // EX_br
        if(!this->EX.br.inst.op.is_nop()){
            std::cout << "     br    : " << this->EX.br.inst.op.to_string() << " (pc=" << this->EX.br.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->EX.br.inst.pc)))) : "") << ")" << (this->EX.br.branch_addr.has_value() ? "\x1b[1m -> taken\x1b[0m" : "\x1b[1m -> untaken\x1b[0m") << std::endl;
        }else{
            std::cout << "     br    :" << std::endl;
        }

        // EX_ma
        if(!this->EX.ma.inst.op.is_nop()){
            std::cout << "     ma    : " << this->EX.ma.inst.op.to_string() << " (pc=" << this->EX.ma.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->EX.ma.inst.pc)))) : "") << ") [state: " << NAMEOF_ENUM(this->EX.ma.state) << (this->EX.ma.state != Configuration::EX_stage::EX_ma::State_ma::Idle ? (", cycle: " + std::to_string(this->EX.ma.cycle_count)) : "") << "]" << (this->EX.ma.available() ? "\x1b[1m\x1b[32m -> available\x1b[0m" : "") << std::endl;
        }else{
            std::cout << "     ma    :" << std::endl;
        }

        // EX_mfp
        if(!this->EX.mfp.inst.op.is_nop()){
            std::cout << "     mfp   : " << this->EX.mfp.inst.op.to_string() << " (pc=" << this->EX.mfp.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->EX.mfp.inst.pc)))) : "") << ") [state: " << NAMEOF_ENUM(this->EX.mfp.state) << (this->EX.mfp.state != Configuration::EX_stage::EX_mfp::State_mfp::Waiting ? (", cycle: " + std::to_string(this->EX.mfp.cycle_count)) : "") << "]" << (this->EX.mfp.available() ? "\x1b[1m\x1b[32m -> available\x1b[0m" : "") << std::endl;
        }else{
            std::cout << "     mfp   :" << std::endl;
        }

        // EX_pfp
        for(unsigned int i=0; i<pipelined_fpu_stage_num; ++i){
            if(!this->EX.pfp.inst[i].op.is_nop()){
                std::cout << "     pfp[" << i << "]: " << this->EX.pfp.inst[i].op.to_string() << " (pc=" << this->EX.pfp.inst[i].pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->EX.pfp.inst[i].pc)))) : "") << ")" << std::endl;
            }else{
                std::cout << "     pfp[" << i << "]:" << std::endl;
            }
        }

        // WB
        std::cout << "\x1b[1m[WB]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            if(this->WB.inst_int[i].has_value()){
                std::cout << (i==0 ? " " : "     ") << "int[" << i << "]: " << this->WB.inst_int[i].value().op.to_string() << " (pc=" << this->WB.inst_int[i].value().pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->WB.inst_int[i].value().pc)))) : "") << ")" << std::endl;
            }else{
                std::cout << (i==0 ? " " : "     ") << "int[" << i << "]:" << std::endl;
            }
        }
        for(unsigned int i=0; i<2; ++i){
            if(this->WB.inst_fp[i].has_value()){
                std::cout << "     fp[" << i << "] : " << this->WB.inst_fp[i].value().op.to_string() << " (pc=" << this->WB.inst_fp[i].value().pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(id_of_pc(this->WB.inst_fp[i].value().pc)))) : "") << ")" << std::endl;
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
Configuration::Hazard_type operator||(Configuration::Hazard_type t1, Configuration::Hazard_type t2){
    if(t1 == Configuration::Hazard_type::No_hazard){
        return t2;
    }else{
        return t1;
    }
}

// 同時発行される命令の間のハザード検出
Configuration::Hazard_type Configuration::intra_hazard_detector(){
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
Configuration::Hazard_type Configuration::inter_hazard_detector(unsigned int i){ // i = 0,1
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
Configuration::Hazard_type Configuration::iwp_hazard_detector(unsigned int i){
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
void Configuration::wb_req(Instruction inst){
    switch(inst.op.type){
        // int
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
        case o_fmvfi:
        case o_jal:
        case o_jalr:
        case o_lw:
        case o_lrd:
        case o_lre:
        case o_ltf:
            if(!this->WB.inst_int[0].has_value()){
                this->WB.inst_int[0] = inst;
            }else if(!this->WB.inst_int[1].has_value()){
                this->WB.inst_int[1] = inst;
            }else{
                exit_with_output("too many requests for WB(int) (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(inst.pc)))) : "") + ")");
            }
            return;
        case o_fadd:
        case o_fsub:
        case o_fmul:
        case o_fdiv:
        case o_fsqrt:
        case o_fcvtif:
        case o_fcvtfi:
        case o_fmvff:
        case o_flw:
        case o_fmvif:
            if(!this->WB.inst_fp[0].has_value()){
                this->WB.inst_fp[0] = inst;
            }else if(!this->WB.inst_fp[1].has_value()){
                this->WB.inst_fp[1] = inst;
            }else{
                exit_with_output("too many requests for WB(int) (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(inst.pc)))) : "") + ")");
            }
            return;
        case o_nop: return;
        default:
            exit_with_output("invalid request for WB (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(inst.pc)))) : "") + ")");
    }
}

bool Configuration::ID_stage::is_not_dispatched(unsigned int i){
    return this->hazard_type[i] != Configuration::Hazard_type::No_hazard;
}

void Configuration::EX_stage::EX_al::exec(){
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
            reg_int.write_int(this->inst.op.rd, this->inst.pc + 4);
            return;
        // jal (pass through)
        case o_jal:
            reg_int.write_int(this->inst.op.rd, this->inst.pc + 4);
            return;
        case o_nop: return;
        default:
            exit_with_output("invalid operation for AL (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

void Configuration::EX_stage::EX_br::exec(){
    switch(this->inst.op.type){
        // branch
        case o_beq:
            if(this->inst.rs1_v.i == this->inst.rs2_v.i){
                this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            }
            ++op_type_count[o_beq];
            return;
        case o_blt:
            if(this->inst.rs1_v.i < this->inst.rs2_v.i){
                this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            }
            ++op_type_count[o_blt];
            return;
        // branch_fp
        case o_fbeq:
            if(this->inst.rs1_v.f == this->inst.rs2_v.f){
                this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            }
            ++op_type_count[o_fbeq];
            return;
        case o_fblt:
            if(this->inst.rs1_v.f < this->inst.rs2_v.f){
                this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            }
            ++op_type_count[o_fblt];
            return;
        // jalr
        case o_jalr:
            this->branch_addr = this->inst.rs1_v.ui; // todo: uiでよい？
            ++op_type_count[o_jalr];
            return;
        // jal
        case o_jal:
            this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            ++op_type_count[o_jal];
            return;
        case o_nop: return;
        default:
            exit_with_output("invalid operation for BR (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

void Configuration::EX_stage::EX_ma::exec(){
    switch(this->inst.op.type){
        case o_sw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                write_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4, this->inst.rs2_v);
            }else{
                exit_with_output("address of store operation should be multiple of 4 [sw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[o_sw];
            return;
        case o_si:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                op_list[(this->inst.rs1_v.i + this->inst.op.imm) / 4] = Operation(this->inst.rs2_v.i);
            }else{
                exit_with_output("address of store operation should be multiple of 4 [si] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[o_si];
            return;
        case o_std:
            send_buffer.push(inst.rs2_v);
            ++op_type_count[o_std];
            return;
        case o_fsw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                write_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4, this->inst.rs2_v);
            }else{
                exit_with_output("address of store operation should be multiple of 4 [fsw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[o_fsw];
            return;
        case o_lw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                reg_int.write_32(this->inst.op.rd, read_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4));
            }else{
                exit_with_output("address of load operation should be multiple of 4 [lw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[o_lw];
            return;
        case o_lre:
            reg_int.write_int(this->inst.op.rd, receive_buffer.empty() ? 1 : 0);
            ++op_type_count[o_lre];
            return;
        case o_lrd:
            if(!receive_buffer.empty()){
                reg_int.write_32(this->inst.op.rd, receive_buffer.front());
                receive_buffer.pop();
            }else{
                exit_with_output("receive buffer is empty [lrd] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[o_lrd];
            return;
        case o_ltf:
            reg_int.write_int(this->inst.op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
            ++op_type_count[o_ltf];
            return;
        case o_flw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                reg_fp.write_32(this->inst.op.rd, read_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4));
            }else{
                exit_with_output("address of load operation should be multiple of 4 [flw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[o_flw];
            return;
        case o_nop: return;
        default:
            exit_with_output("invalid operation for MA (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

bool Configuration::EX_stage::EX_ma::available(){
    if(is_quick){
        return true;
    }else{
        return this->cycle_count == 2; // 仮の値
    }
}

void Configuration::EX_stage::EX_mfp::exec(){
    switch(this->inst.op.type){
        // op_fp
        case o_fdiv:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, this->inst.rs1_v.f / this->inst.rs2_v.f);
            }else{
                reg_fp.write_32(this->inst.op.rd, fdiv(this->inst.rs1_v, this->inst.rs2_v));
            }
            ++op_type_count[o_fdiv];
            return;
        case o_fsqrt:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, std::sqrt(this->inst.rs1_v.f));
            }else{
                reg_fp.write_32(this->inst.op.rd, fsqrt(this->inst.rs1_v));
            }
            ++op_type_count[o_fsqrt];
            return;
        case o_fcvtif:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, static_cast<float>(this->inst.rs1_v.i));
            }else{
                reg_fp.write_32(this->inst.op.rd, itof(this->inst.rs1_v));
            }
            ++op_type_count[o_fcvtif];
            return;
        case o_fcvtfi:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, static_cast<int>(std::nearbyint(this->inst.rs1_v.f)));
            }else{
                reg_fp.write_32(this->inst.op.rd, ftoi(this->inst.rs1_v));
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
        case o_nop: return;
        default:
            exit_with_output("invalid operation for mFP (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

bool Configuration::EX_stage::EX_mfp::available(){
    if(is_quick){
        return true;
    }else{
        return this->cycle_count == 2; // 仮の値
    }
}

void Configuration::EX_stage::EX_pfp::exec(){
    Instruction inst = this->inst[pipelined_fpu_stage_num-1];
    switch(inst.op.type){
        case o_fadd:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f + inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fadd(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fadd];
            return;
        case o_fsub:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f - inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fsub(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fsub];
            return;
        case o_fmul:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f * inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fmul(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fmul];
            return;
        case o_nop: return;
        default:
            exit_with_output("invalid operation for pFP (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(inst.pc)))) : "") + ")");
    }
}

// EXステージに命令がないかどうかの判定
bool Configuration::EX_stage::is_clear(){
    bool pfp_clear = true;
    for(unsigned int i=0; i<pipelined_fpu_stage_num; ++i){
        if(!this->pfp.inst[i].op.is_nop()){
            pfp_clear = false;
            break;
        }
    }
    return (this->als[0].inst.op.is_nop() && this->als[1].inst.op.is_nop() && this->br.inst.op.is_nop() && this->ma.inst.op.is_nop() && this->mfp.inst.op.is_nop() && pfp_clear);
}
