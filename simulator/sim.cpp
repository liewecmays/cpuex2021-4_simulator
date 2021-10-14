#include "sim.hpp"
#include "util.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <unistd.h>

// グローバル変数
std::vector<Operation> op_list;
std::vector<int> reg_list(32);
std::vector<float> reg_fp_list(32);
std::vector<int> memory;
unsigned int current_pc = 0; // 注意: 行番号と異なり0-indexed
bool is_debug = false;
bool simulation_end = false;
int op_count = 0;

// 機械語命令をパース
Operation parse_op(std::string line){
    Operation op;
    int opcode;

    opcode = std::stoi(line.substr(0, 4), 0, 2);
    op.opcode = opcode;
    switch(opcode){
        case 0: // op
        case 1: // op_fp
            op.funct = std::stoi(line.substr(4, 3), 0, 2);
            op.rs1 = std::stoi(line.substr(7, 5), 0, 2);
            op.rs2 = std::stoi(line.substr(12, 5), 0, 2);
            op.rd = std::stoi(line.substr(17, 5), 0, 2);
            op.imm = -1;
            break;
        case 2: // branch
        case 3: // store
        case 4: // store_fp
            op.funct = std::stoi(line.substr(4, 3), 0, 2);
            op.rs1 = std::stoi(line.substr(7, 5), 0, 2);
            op.rs2 = std::stoi(line.substr(12, 5), 0, 2);
            op.rd = -1;
            op.imm = binary_stoi(line.substr(17, 15));
            break;
        case 5: // op_imm
        case 6: // load
        case 7: // load_fp
            op.funct = std::stoi(line.substr(4, 3), 0, 2);
            op.rs1 = std::stoi(line.substr(7, 5), 0, 2);
            op.rs2 = -1;
            op.rd = std::stoi(line.substr(17, 5), 0, 2);
            op.imm = binary_stoi(line.substr(12, 5) + line.substr(22, 10));
            break;
        case 8: // jalr
            op.funct = -1;
            op.rs1 = std::stoi(line.substr(7, 5), 0, 2);
            op.rs2 = -1;
            op.rd = std::stoi(line.substr(17, 5), 0, 2);
            op.imm = binary_stoi(line.substr(4, 3) + line.substr(12, 5) + line.substr(22, 10));
            break;
        case 9: // jal
            op.funct = -1;
            op.rs1 = -1;
            op.rs2 = -1;
            op.rd = std::stoi(line.substr(17, 5), 0, 2);
            op.imm = binary_stoi(line.substr(4, 13) + line.substr(22, 10));
            break;
        // case 10: // lui
        //     op.funct = -1;
        //     op.rs1 = -1;
        //     op.rs2 = -1;
        //     op.imm = binary_stoi(line.substr(4, 28));
        //     break;
        default:
            std::cerr << "Error in parsing the code" << std::endl;
            std::exit(EXIT_FAILURE);
    }

    return op;
}

// 命令を実行し、PCを変化させる
void exec_op(Operation &op){
    int load = 0;
    switch(op.opcode){
        case 0: // op
            switch(op.funct){
                case 0: // add
                    write_reg(op.rd, read_reg(op.rs1) + read_reg(op.rs2));
                    current_pc++;
                    return;
                case 1: // sub
                    write_reg(op.rd, read_reg(op.rs1) - read_reg(op.rs2));
                    current_pc++;
                    return;
                case 2: // sll
                    write_reg(op.rd, read_reg(op.rs1) << read_reg(op.rs2));
                    current_pc++;
                    return;
                case 3: // srl
                    write_reg(op.rd, static_cast<unsigned int>(read_reg(op.rs1)) >> read_reg(op.rs2));
                    current_pc++;
                    return;
                case 4: // sra
                    write_reg(op.rd, read_reg(op.rs1) >> read_reg(op.rs2)); // todo: 処理系依存
                    current_pc++;
                    return;
                default: break;
            }
            break;
        // case 1: // op_fp
        //     break;
        case 2: // branch
            switch(op.funct){
                case 0: // beq
                    read_reg(op.rs1) == read_reg(op.rs2) ? current_pc += op.imm : current_pc++;
                    return;
                case 1: // blt
                    read_reg(op.rs1) < read_reg(op.rs2) ? current_pc += op.imm : current_pc++;
                    return;
                case 2: // ble
                    read_reg(op.rs1) <= read_reg(op.rs2) ? current_pc += op.imm : current_pc++;
                    return;
                default: break;
            }
            break;
        case 3: // store
            switch(op.funct){
                case 0: // sw
                    for(int i=0; i<4; i++){
                        memory[read_reg(op.rs1) + op.imm + i] = (read_reg(op.rs2) & (255 << (8 * i))) / (1 << (8 * i));
                    }
                    current_pc++;
                    return;
                default: break;
            }
            break;
        // case 4: // store_fp
        //     current_pc++;
        //     break;
        case 5: // op_imm
            switch(op.funct){
                case 0: // addi
                    write_reg(op.rd, read_reg(op.rs1) + op.imm);
                    current_pc++;
                    return;
                case 2: // slli
                    write_reg(op.rd, read_reg(op.rs1) << op.imm);
                    current_pc++;
                    return;
                case 3: // srli
                    write_reg(op.rd, static_cast<unsigned int>(read_reg(op.rs1)) >> op.imm);
                    current_pc++;
                    return;
                case 4: // srai
                    write_reg(op.rd, read_reg(op.rs1) >> op.imm); // todo: 処理系依存
                    current_pc++;
                    return;
                default: break;
            }
            break;
        case 6: // load
            switch(op.funct){
                case 0: // lw
                    for(int i=0; i<4; i++){
                        load += memory[read_reg(op.rs1) + op.imm + i] << (8 * i);
                    }
                    write_reg(op.rd, load);
                    current_pc++;
                    return;
                default: break;
            }
            break;
        // case 7: // load_fp
        //     current_pc++;
        //     return;
        case 8: // jalr
            write_reg(op.rd, current_pc + 1);
            current_pc = read_reg(op.rs1) + op.imm;
            // todo: current_pc = read_reg(op.rs1) + op.imm * 4;
            return;
        case 9: // jal
            write_reg(op.rd, current_pc + 1);
            current_pc = current_pc + op.imm;
            return;
        // case 10: // lui
        //     current_pc++;
        //     break;
        default: break;
    }

    std::cerr << "Error in executing the code" << std::endl;
    std::exit(EXIT_FAILURE);
}

// デバッグモードのコマンドを認識して実行
bool exec_command(std::string cmd){
    bool res = false; // デバッグモード終了ならtrue
    std::smatch match;

    if(std::regex_match(cmd, std::regex("^\\s*\\r?\\n?$"))){ // 空行
        // do nothing
    }else if(std::regex_match(cmd, std::regex("^\\s*(q|(quit))\\s*$"))){ // quit
        res = true;
    }else if(std::regex_match(cmd, std::regex("^\\s*(h|(help))\\s*$"))){ // help
        // todo: help
    }else if(std::regex_match(cmd, std::regex("^\\s*(d|(do))\\s*$"))){ // do
        if(simulation_end){
            std::cout << "No operation is left to be simulated." << std::endl;
        }else{
            bool end_flag = false;
            if(is_end(op_list[current_pc])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
            std::cout << "op[" << current_pc << "]: " << string_of_op (op_list[current_pc]) << std::endl;
            exec_op(op_list[current_pc]);
            if(current_pc >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
            op_count++;

            if(end_flag){
                simulation_end = true;
                std::cout << "All operations have been simulated successfully!" << std::endl;
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(do))\\s+(\\d+)\\s*$"))){ // do N
        if(simulation_end){
            std::cout << "No operation is left to be simulated." << std::endl;
        }else{
            bool end_flag = false;
            for(int i=0; i<std::stoi(match[3].str()); i++){
                if(is_end(op_list[current_pc])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                exec_op(op_list[current_pc]);
                if(current_pc >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                op_count++;
                
                if(end_flag){
                    simulation_end = true;
                    std::cout << "All operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(r|(run))\\s*$"))){ // run
        if(simulation_end){
            std::cout << "No operation is left to be simulated." << std::endl;
        }else{
            bool end_flag = false;
            while(true){
                if(is_end(op_list[current_pc])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                exec_op(op_list[current_pc]);
                if(current_pc >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                op_count++;
                
                if(end_flag){
                    simulation_end = true;
                    std::cout << "All operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }
    }else{
        std::cout << "Error: invalid command" << std::endl;
    }

    return res;
}


int main(int argc, char *argv[]){
    // todo: 実行環境における型のバイト数などの確認
    
    // std::cout << "===== simulation start =====" << std::endl;

    // コマンドライン引数をパース
    int option;
    std::string filename;
    while ((option = getopt(argc, argv, "f:d")) != -1){
        switch(option){
            case 'f':
                filename = "./code/" + std::string(optarg);
                break;
            case 'd':
                is_debug = true;
                break;
            default:
                std::cerr << "Invalid command-line argument" << std::endl;
                std::exit(EXIT_FAILURE);
        }
    }

    // メモリ領域の確保
    memory.reserve(1000); // todo: サイズを入力で変更できるようにする

    // ファイルを読む
    std::ifstream input_file(filename);
    if(!input_file.is_open()){
        std::cerr << "Could not open ./code/" << filename << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // ファイルの各行をパースしてop_listに追加
    std::string line;
    while(std::getline(input_file, line)){
        if(std::regex_match(line, std::regex("^\\s*\\r?\\n?$"))){ // 空行は無視
            continue;
        }else{
            op_list.emplace_back(parse_op(line));
        }
    }
    
    if(is_debug){ // デバッグモード
        std::string cmd;
        while(true){
            std::cout << "# " << std::ends;    
            std::getline(std::cin, cmd);
            if(exec_command(cmd)) break;
        }
    }else{ // デバッグなしモード
        exec_command("run");
    }
}
