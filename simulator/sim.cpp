#include "sim.hpp"
#include "common.hpp"
#include "util.hpp"
#include "op.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <boost/bimap/bimap.hpp>
#include <thread>
#include <boost/asio.hpp>
#include <regex>
#include <unistd.h>
#include <iomanip>
#include <bitset>

namespace asio = boost::asio;
using asio::ip::tcp;

/* グローバル変数 */
// 内部処理関係
std::vector<Operation> op_list; // 命令のリスト(PC順)
std::vector<int> reg_list(32); // 整数レジスタのリスト
std::vector<float> reg_fp_list(32); // 浮動小数レジスタのリスト
std::vector<Bit32> memory; // メモリ領域

unsigned int pc = 0; // プログラムカウンタ
int op_count = 0; // 命令のカウント

int port = 8000; // 通信に使うポート番号
std::queue<Bit32> receive_buffer; // 外部通信での受信バッファ

// シミュレーションの制御
bool is_debug = false; // デバッグモード
bool is_out = false; // 出力モード
std::string output_filename; // 出力用のファイル名
std::stringstream output; // 出力内容

// 処理用のデータ構造
bimap_t bp_to_id; // ブレークポイントと命令idの対応
bimap_t label_to_id; // ラベルと命令idの対応
bimap_t2 id_to_line; // 命令idと行番号の対応

// フラグ
bool simulation_end = false; // シミュレーション終了判定
bool breakpoint_skip = false; // ブレークポイント直後の停止回避
bool loop_flag = false;

// ターミナルへの出力用
std::string head = "\x1b[1m[sim]\x1b[0m ";


int main(int argc, char *argv[]){
    // todo: 実行環境における型のバイト数などの確認

    std::cout << head << "simulation start" << std::endl;

    // コマンドライン引数をパース
    int option;
    std::string filename;
    while ((option = getopt(argc, argv, "f:odp:")) != -1){
        switch(option){
            case 'f':
                filename = std::string(optarg);
                break;
            case 'o':
                is_out = true;
                std::cout << head << "output mode enabled" << std::endl;
                break;
            case 'd':
                is_debug = true;
                std::cout << head << "entering debug mode ..." << std::endl;
                break;
            case 'p':
                port = std::stoi(std::string(optarg));
                break;
            default:
                std::cerr << head_error << "Invalid command-line argument" << std::endl;
                std::exit(EXIT_FAILURE);
        }
    }

    // メモリ領域の確保
    memory.reserve(1000); // todo: サイズを入力で変更できるようにする

    // ファイルを読む
    std::string input_filename = "./code/" + filename + (is_debug ? ".dbg" : "");
    std::ifstream input_file(input_filename);
    if(!input_file.is_open()){
        std::cerr << head_error << "could not open " << input_filename << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // ファイルの各行をパースしてop_listに追加
    std::string code;
    int code_id = 0; // 0-indexed
    while(std::getline(input_file, code)){
        if(std::regex_match(code, std::regex("^\\s*\\r?\\n?$"))){ // 空行は無視
            continue;
        }else{
            op_list.emplace_back(parse_op(code));
            
            // ラベル・ブレークポイントの処理
            if(code.size() > 32){
                if(is_debug){ // デバッグモード
                    std::smatch match;
                    if(std::regex_match(code, match, std::regex("^\\d{32}@(\\d+)$"))){
                        id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                    }else if(std::regex_match(code, match, std::regex("^\\d{32}@(\\d+)#(([a-zA-Z_]\\w*(.\\d+)?))$"))){ // ラベルのみ
                        id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                        label_to_id.insert(bimap_value_t(match[2].str(), code_id));             
                    }else if(std::regex_match(code, match, std::regex("^\\d{32}@(\\d+)!(([a-zA-Z_]\\w*(.\\d+)?))$"))){ // ブレークポイントのみ
                        id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                        bp_to_id.insert(bimap_value_t(match[2].str(), code_id));
                    }else if(std::regex_match(code, match, std::regex("^\\d{32}@(\\d+)#(([a-zA-Z_]\\w*(.\\d+)?))!(([a-zA-Z_]\\w*(.\\d+)?))$"))){ // ラベルとブレークポイントの両方
                        id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                        label_to_id.insert(bimap_value_t(match[2].str(), code_id));
                        bp_to_id.insert(bimap_value_t(match[3].str(), code_id));
                    }else{
                        std::cerr << head_error << "could not parse the code" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                }else{ // デバッグモードでないのにラベルやブレークポイントの情報が入っている場合エラー
                    std::cerr << head_error << "could not parse the code (maybe it is encoded in debug-style)" << std::endl;
                    std::exit(EXIT_FAILURE);
                }
            }

            code_id++;
        }
    }

    // コマンドの受け付けとデータ受信処理を別々のスレッドで起動
    std::thread t1(simulate);
    std::thread t2(receive);
    t1.join();
    t2.detach();

    // 実行結果の情報を出力
    if(is_out){
        time_t t = time(nullptr);
        tm* time = localtime(&t);
        std::stringstream timestamp;
        timestamp << "20" << time -> tm_year - 100;
        timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_mon + 1;
        timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_mday;
        timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_hour;
        timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_min;
        timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_sec;
        output_filename = "./result/" + filename + (is_debug ? "-dbg" : "") + "_" + timestamp.str() + ".txt";
        std::ofstream output_file(output_filename);
        if(!output_file){
            std::cerr << head_error << "could not open " << output_filename << std::endl;
            std::exit(EXIT_FAILURE);
        }
        output_file << output.str();

        std::string output_filename2 = "./result/" + filename + (is_debug ? "-dbg" : "") + "_dump_" + timestamp.str() + ".txt";
        std::ofstream output_file2(output_filename2);
        if(!output_file){
            std::cerr << head_error << "could not open " << output_filename << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::streambuf* strbuf = std::cout.rdbuf(output_file2.rdbuf());
        print_memory(0, 1000);
        std::cout.rdbuf(strbuf);
    }
}


// シミュレーションの本体処理
void simulate(){
    if(is_debug){ // デバッグモード
        std::string cmd;
        while(true){
            std::cout << "# " << std::ends;    
            if(!std::getline(std::cin, cmd)) break;
            if(exec_command(cmd)) break;
        }
    }else{ // デバッグなしモード
        exec_command("f");
    }

    return;
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
        breakpoint_skip = false;
        if(simulation_end){
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }else{
            bool end_flag = false;
            if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
            std::cout << "pc " << pc << " (line " << id_to_line.left.at(id_of_pc(pc)) << ") " << string_of_op(op_list[id_of_pc(pc)]) << std::endl;
            exec_op(op_list[id_of_pc(pc)]);
            if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
            op_count++;

            if(end_flag){
                simulation_end = true;
                std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(do))\\s+(\\d+)\\s*$"))){ // do N
        breakpoint_skip = false;
        if(simulation_end){
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }else{
            bool end_flag = false;
            for(int i=0; i<std::stoi(match[3].str()); i++){
                if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                exec_op(op_list[id_of_pc(pc)]);
                if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                op_count++;
                
                if(end_flag){
                    simulation_end = true;
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(f|(finish))\\s*$"))){ // finish
        loop_flag = true;
        breakpoint_skip = false;
        if(simulation_end){
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }else{
            bool end_flag = false;
            while(true){
                if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                exec_op(op_list[id_of_pc(pc)]);
                if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                op_count++;
                
                if(end_flag){
                    simulation_end = true;
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(init))\\s*$"))){ // init
        breakpoint_skip = false;
        simulation_end = false;
        pc = 0; // PCを0にする
        op_count = 0; // 総実行命令数を0にする
        for(int i=0; i<32; i++){ // レジスタをクリア
            reg_list[i] = 0;
            reg_fp_list[i] = 0;
        }
        for(int i=0; i<1000; i++){ // メモリをクリア
            memory[i] = Bit32(0);
        }

        std::cout << head_info << "simulation environment is now initialized" << std::endl;
    }else if(std::regex_match(cmd, std::regex("^\\s*(c|(continue))\\s*$"))){ // continue
        if(simulation_end){
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }else{
            loop_flag = true;
            bool end_flag = false;
            while(true){
                if(bp_to_id.right.find(id_of_pc(pc)) != bp_to_id.right.end()){ // ブレークポイントに当たった場合は停止
                    if(breakpoint_skip){
                        breakpoint_skip = false;
                    }else{
                        std::cout << head_info << "halt before breakpoint '" + bp_to_id.right.at(id_of_pc(pc)) << "' (line " << id_to_line.left.at(id_of_pc(pc)) << ")" << std::endl;
                        loop_flag = false;
                        breakpoint_skip = true; // ブレークポイント直後に再度continueした場合はスキップ
                        break;
                    }
                }

                if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                exec_op(op_list[id_of_pc(pc)]);
                if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                op_count++;
                
                if(end_flag){
                    simulation_end = true;
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(c|(continue))\\s+(([a-zA-Z_]\\w*(.\\d+)?))\\s*$"))){ // continue break
        if(simulation_end){
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }else{
            loop_flag = true;
            std::string bp = match[3].str();
            if(bp_to_id.left.find(bp) != bp_to_id.left.end()){
                unsigned int bp_id = bp_to_id.left.at(bp);
                bool end_flag = false;
                while(true){
                    if(id_of_pc(pc) == bp_id){
                        if(breakpoint_skip){
                            breakpoint_skip = false;
                        }else{
                            std::cout << head_info << "halt before breakpoint '" + bp << "' (line " << id_of_pc(pc) + 1 << ")" << std::endl;
                            loop_flag = false;
                            breakpoint_skip = true; // ブレークポイント直後に再度continueした場合はスキップ
                            break;
                        }
                    }

                    if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                    exec_op(op_list[id_of_pc(pc)]);
                    if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                    op_count++;
                    
                    if(end_flag){
                        simulation_end = true;
                        std::cout << head_info << "did not encounter breakpoint '" << bp << "'" << std::endl;
                        std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                        break;
                    }
                }
            }else{
                std::cout << "breakpoint '" << bp << "' has not been set" << std::endl;
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(info))\\s*$"))){ // info
        std::cout << "operations executed: " << op_count << std::endl;
        if(simulation_end){
            std::cout << "next: (no operation left to be simulated)" << std::endl;
        }else{
            std::cout << "next: pc " << pc << " (line " << id_to_line.left.at(id_of_pc(pc)) << ") " << string_of_op(op_list[id_of_pc(pc)]) << std::endl;
        }
        if(bp_to_id.empty()){
            std::cout << "breakpoints: (no breakpoint found)" << std::endl;
        }else{
            std::cout << "breakpoints:" << std::endl;
            for(auto x : bp_to_id.left) {
                std::cout << "  " << x.first << " (pc " << x.second * 4 << ", line " << id_to_line.left.at(x.second) << ")" << std::endl;
            }
        }
    // }else if(std::regex_match(cmd, std::regex("^\\s*(p|(print))\\s*$"))){ // print
    //
    }else if(std::regex_match(cmd, std::regex("^\\s*(p|(print))\\s+reg\\s*$"))){ // print reg
        print_reg();
        print_reg_fp();
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))(\\s+(x|f)(\\d+))+\\s*$"))){ // print reg
        int reg_no;
        while(std::regex_search(cmd, match, std::regex("(x|f)(\\d+)"))){
            reg_no = std::stoi(match[2].str());
            if(match[1].str() == "x"){ // int
                std::cout << "\x1b[1m%x" << reg_no << "\x1b[0m: " << reg_list[reg_no] << std::endl;
            }else{ // float
                std::cout << "\x1b[1m%f" << reg_no << "\x1b[0m: " << std::setprecision(10) << reg_fp_list[reg_no] << std::endl;
            }
            cmd = match.suffix();
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))\\s+(m|mem)\\[(\\d+):(\\d+)\\]\\s*$"))){ // print mem[N:M]
        int start = std::stoi(match[4].str());
        int width = std::stoi(match[5].str());
        print_memory(start, width);
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(s|(set))\\s+(x(\\d+))\\s+(\\d+)\\s*$"))){ // set reg N
        int reg_no = std::stoi(match[4].str());
        int val = std::stoi(match[5].str());
        if(0 < reg_no && reg_no < 31){
            write_reg(reg_no, val);
        }else{
            std::cout << head_error << "invalid argument (integer registers are x0,...,x31)" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(b|(break))\\s+(([a-zA-Z_]\\w*(.\\d+)?))\\s*$"))){ // break label
        std::string label = match[3].str();
        if(label_to_id.left.find(label) != label_to_id.left.end()){
            if(bp_to_id.left.find(label) == bp_to_id.left.end()){
                int label_id = label_to_id.left.at(label); // 0-indexed
                bp_to_id.insert(bimap_value_t(label, label_id));
                std::cout << head_info << "breakpoint '" << label << "' is now set (at pc " << label_id * 4 << ", line " << id_to_line.left.at(label_id) << ")" << std::endl;
            }else{
                std::cout << head_error << "breakpoint '" << label << "' has already been set" << std::endl;  
            }
        }else{
            std::cout << head_error << "label '" << label << "' is not found" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(b|(break))\\s+(\\d+)\\s+(([a-zA-Z_]\\w*(.\\d+)?))\\s*$"))){ // break N id (Nはアセンブリコードの行数)
        unsigned int line_no = std::stoi(match[3].str());
        std::string bp = match[4].str();
        if(id_to_line.right.find(line_no) != id_to_line.right.end()){ // 行番号は命令に対応している？
            unsigned int id = id_to_line.right.at(line_no);
            if(bp_to_id.right.find(id) == bp_to_id.right.end()){ // idはまだブレークポイントが付いていない？
                if(label_to_id.right.find(id) == label_to_id.right.end()){ // idにはラベルが付いていない？
                    if(bp_to_id.left.find(bp) == bp_to_id.left.end()){ // そのブレークポイント名は使われていない？
                        if(label_to_id.left.find(bp) == label_to_id.left.end()){ // そのブレークポイント名はラベル名と重複していない？
                            bp_to_id.insert(bimap_value_t(bp, id));
                            std::cout << head_info << "breakpoint '" << bp << "' is now set to line " << line_no << std::endl;
                        }else{
                            std::cout << head_error << "'" << bp << "' is a label name and cannot be used as a breakpoint id" << std::endl;
                        }
                    }else{
                        std::cout << head_error << "breakpoint id '" << bp << "' has already been used for another line" << std::endl;
                    } 
                }else{
                    std::string label = label_to_id.right.at(id);
                    std::cout << head_error << "line " << line_no << " is labeled '" << label << "' (hint: exec 'break " << label << "')" << std::endl;
                }   
            }else{
                std::cout << head_error << "a breakpoint has already been set to line " << line_no << std::endl;
            }
        }else{
            std::cout << head_error << "invalid line number" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(delete))\\s+(([a-zA-Z_]\\w*(.\\d+)?))\\s*$"))){ // delete id
        std::string bp_id = match[3].str();
        if(bp_to_id.left.find(bp_id) != bp_to_id.left.end()){
            bp_to_id.left.erase(bp_id);
            std::cout << head_info << "breakpoint '" << bp_id << "' is now deleted" << std::endl;
        }else{
            std::cout << head_error << "breakpoint '" << bp_id << "' has not been set" << std::endl;  
        }
    }else{
        std::cout << head_error << "invalid command" << std::endl;
    }

    return res;
}

// 命令を実行し、PCを変化させる
void exec_op(Operation &op){
    if(is_out){
        if(is_debug){
            output << op_count << ": pc " << pc << ", line " << id_to_line.left.at(id_of_pc(pc)) << " (" << string_of_op(op) << ")\n";
        }else{
            output << op_count << ": pc " << pc << " (" << string_of_op(op) << ")\n";
        }
    }

    switch(op.opcode){
        case 0: // op
            switch(op.funct){
                case 0: // add
                    write_reg(op.rd, read_reg(op.rs1) + read_reg(op.rs2));
                    pc += 4;
                    return;
                case 1: // sub
                    write_reg(op.rd, read_reg(op.rs1) - read_reg(op.rs2));
                    pc += 4;
                    return;
                case 2: // sll
                    write_reg(op.rd, read_reg(op.rs1) << read_reg(op.rs2));
                    pc += 4;
                    return;
                case 3: // srl
                    write_reg(op.rd, static_cast<unsigned int>(read_reg(op.rs1)) >> read_reg(op.rs2));
                    pc += 4;
                    return;
                case 4: // sra
                    write_reg(op.rd, read_reg(op.rs1) >> read_reg(op.rs2)); // todo: 処理系依存
                    pc += 4;
                    return;
                case 5: // andi
                    write_reg(op.rd, read_reg(op.rs1) & read_reg(op.rs2));
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 1: // op_fp todo: 仕様に沿っていないので注意
            switch(op.funct){
                case 0: // fadd
                    write_reg_fp(op.rd, read_reg_fp(op.rs1) + read_reg_fp(op.rs2));
                    pc += 4;
                    return;
                case 1: // fsub
                    write_reg_fp(op.rd, read_reg_fp(op.rs1) - read_reg_fp(op.rs2));
                    pc += 4;
                    return;
                case 2: // fmul
                    write_reg_fp(op.rd, read_reg_fp(op.rs1) * read_reg_fp(op.rs2));
                    pc += 4;
                    return;
                case 3: // fdiv
                    write_reg_fp(op.rd, read_reg_fp(op.rs1) / read_reg_fp(op.rs2));
                    pc += 4;
                    return;
                case 4: // fsqrt
                    write_reg_fp(op.rd, std::sqrt(read_reg_fp(op.rs1)));
                    pc += 4;
                    return;
            }
            break;
        case 2: // branch
            switch(op.funct){
                case 0: // beq
                    read_reg(op.rs1) == read_reg(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                case 1: // blt
                    read_reg(op.rs1) < read_reg(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                case 2: // ble
                    read_reg(op.rs1) <= read_reg(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                default: break;
            }
            break;
        case 3: // branch_fp
            switch(op.funct){
                case 0: // fbeq
                    read_reg_fp(op.rs1) == read_reg_fp(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                case 1: // fblt
                    read_reg_fp(op.rs1) < read_reg_fp(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                default: break;
            }
            break;
        case 4: // store
            switch(op.funct){
                case 0: // sw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        memory[(read_reg(op.rs1) + op.imm) / 4] = Bit32(read_reg(op.rs2));
                    }else{
                        std::cerr << head_error << "address of store operation should be multiple of 4" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 1: // si
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        std::stringstream code;
                        code << std::bitset<32>(read_reg(op.rs2));
                        op_list[(read_reg(op.rs1) + op.imm) / 4] = parse_op(code.str());
                    }else{
                        std::cerr << head_error << "address of store operation should be multiple of 4" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 2: // std
                    {
                        asio::io_service io_service;
                        tcp::socket socket(io_service);
                        boost::system::error_code e;

                        socket.connect(tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port+1), e);
                        if(e){
                            std::cout << head_error << "connection failed (" << e.message() << ")" << std::endl;
                            std::exit(EXIT_FAILURE);
                        }
                        
                        asio::write(socket, asio::buffer(data_of_int(read_reg(op.rs2))), e);
                        if(e){
                            std::cout << head_error << "data transmission failed (" << e.message() << ")" << std::endl;
                            std::exit(EXIT_FAILURE);
                        }

                        if(is_debug){
                            std::cout << head_data << "sent " << read_reg(op.rs2) << std::endl;
                            if(!loop_flag){
                                std::cout << "# " << std::flush;
                            }
                        }

                        socket.close();
                    }
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 5: // store_fp
            switch(op.funct){
                case 0: // fsw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        memory[(read_reg(op.rs1) + op.imm) / 4] = Bit32(read_reg_fp(op.rs2));
                    }else{
                        std::cerr << head_error << "address of store operation should be multiple of 4" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 2: // fstd
                    {
                        asio::io_service io_service;
                        tcp::socket socket(io_service);
                        boost::system::error_code e;

                        socket.connect(tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port+1));
                        if(e){
                            std::cout << head_error << "connection failed (" << e.message() << ")" << std::endl;
                            std::exit(EXIT_FAILURE);
                        }
                        
                        asio::write(socket, asio::buffer(data_of_float(read_reg_fp(op.rs2))), e);
                        if(e){
                            std::cout << head_error << "data transmission failed (" << e.message() << ")" << std::endl;
                            std::exit(EXIT_FAILURE);
                        }

                        if(is_debug){
                            std::cout << head_data << "sent " << read_reg_fp(op.rs2) << std::endl;
                            if(!loop_flag){
                                std::cout << "# " << std::flush;
                            }
                        }

                        socket.close();
                    }
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 6: // op_imm
            switch(op.funct){
                case 0: // addi
                    write_reg(op.rd, read_reg(op.rs1) + op.imm);
                    pc += 4;
                    return;
                case 2: // slli
                    write_reg(op.rd, read_reg(op.rs1) << op.imm);
                    pc += 4;
                    return;
                case 3: // srli
                    write_reg(op.rd, static_cast<unsigned int>(read_reg(op.rs1)) >> op.imm);
                    pc += 4;
                    return;
                case 4: // srai
                    write_reg(op.rd, read_reg(op.rs1) >> op.imm); // todo: 処理系依存
                    pc += 4;
                    return;
                case 5: // andi
                    write_reg(op.rd, read_reg(op.rs1) & op.imm);
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 7: // load
            switch(op.funct){
                case 0: // lw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        write_reg(op.rd, memory[(read_reg(op.rs1) + op.imm) / 4].to_int());
                    }else{
                        std::cerr << head_error << "address of load operation should be multiple of 4" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 1: // lre
                    write_reg(op.rd, receive_buffer.empty() ? 1 : 0);
                    pc += 4;
                    return;
                case 2: // lrd
                    if(!receive_buffer.empty()){
                        write_reg(op.rd, receive_buffer.front().to_int());
                        receive_buffer.pop();
                    }else{
                        std::cerr << head_error << "receive buffer is empty" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 3: // ltf
                    write_reg(op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 8: // load_fp
            switch(op.funct){
                case 0: // flw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        write_reg_fp(op.rd, memory[(read_reg(op.rs1) + op.imm) / 4].to_float());
                    }else{
                        std::cerr << head_error << "address of load operation should be multiple of 4" << std::endl;
                    }
                    pc += 4;
                    return;
                case 2: // lrd
                    if(!receive_buffer.empty()){
                        write_reg(op.rd, receive_buffer.front().to_float());
                        receive_buffer.pop();
                    }else{
                        std::cerr << head_error << "receive buffer is empty" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 9: // jalr
            write_reg(op.rd, pc + 4);
            pc = read_reg(op.rs1) + op.imm * 4;
            return;
        case 10: // jal
            write_reg(op.rd, pc + 4);
            pc += op.imm * 4;
            return;
        case 11: // long_imm
            switch(op.funct){
                case 0: // lui
                    write_reg(op.rd, op.imm << 12);
                    pc += 4;
                    return;
                case 1: // auipc
                    write_reg(op.rd, pc + (op.imm << 12));
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 12: // itof
            switch(op.funct){
                Int_float u;
                case 0: // fmv.i.f
                    u.i = read_reg(op.rs1);
                    write_reg_fp(op.rd, u.f);
                    pc += 4;
                    return;
                case 5: // fcvt.i.f
                    write_reg_fp(op.rd, static_cast<float>(read_reg(op.rs1)));
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 13: // ftoi
            switch(op.funct){
                Int_float u;
                case 0: // fmv.f.i
                    u.f = read_reg_fp(op.rs1);
                    write_reg(op.rd, u.i);
                    pc += 4;
                    return;
                case 6: // fcvt.f.i
                    write_reg(op.rd, static_cast<int>(read_reg_fp(op.rs1)));
                    pc += 4;
                    return;
                default: break;
            }
            break;
        default: break;
    }

    std::cerr << head_error << "error in executing the code" << std::endl;
    std::exit(EXIT_FAILURE);
}

// データの受信
void receive(){
    asio::io_service io_service;
    tcp::acceptor acc(io_service, tcp::endpoint(tcp::v4(), port));
    tcp::socket socket(io_service);

    boost::system::error_code e;
    while(true){
        acc.accept(socket, e);
        if(e){
            std::cerr << head_error << "connection failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        asio::streambuf buf;
        asio::read(socket, buf, asio::transfer_all(), e);

        if(e && e != asio::error::eof){
            std::cerr << head_error << "data reception failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        std::string data = asio::buffer_cast<const char*>(buf.data());
        if(is_debug){
            std::cout << head_data << "received " << data << std::endl;
            if(!loop_flag){
                std::cout << "# " << std::flush;
            }
        }
        receive_buffer.push(bit32_of_data(data));

        socket.close();
    }
    
    return;
}

// PCから命令IDへの変換(4の倍数になっていない場合エラー)
unsigned int id_of_pc(unsigned int n){
    if(n % 4 == 0){
        return n / 4;
    }else{
        std::cerr << head_error << "error with program counter" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

// 整数レジスタから読む
int read_reg(int i){
    return i == 0 ? 0 : reg_list[i];
}

// 整数レジスタに書き込む
void write_reg(int i, int v){
    if (i != 0) reg_list[i] = v;
    return;
}

// 浮動小数点数レジスタから読む
float read_reg_fp(int i){
    return i == 0 ? 0 : reg_fp_list[i];
}

// 浮動小数点数レジスタに書き込む
void write_reg_fp(int i, float v){
    if (i != 0) reg_fp_list[i] = v;
    return;
}

// 整数レジスタの内容を表示
void print_reg(){
    for(int i=0; i<32; i++){
        std::cout << "\x1b[1mx" << i << "\x1b[0m:" << std::ends;
        if(i < 10) std::cout << " " << std::ends;
        std::cout.setf(std::ios::hex, std::ios::basefield);
        std::cout.fill('0');
        std::cout << std::setw(8) << reg_list[i] << " " << std::ends;
        std::cout.setf(std::ios::dec, std::ios::basefield);
        if(i % 4 == 3) std::cout << std::endl;
    }
    return;
}

// 浮動小数点数レジスタの内容を表示
void print_reg_fp(){
    for(int i=0; i<32; i++){
        std::cout << "\x1b[1mf" << i << "\x1b[0m:" << std::ends;
        if(i < 10) std::cout << " " << std::ends;
        std::cout.setf(std::ios::hex, std::ios::basefield);
        std::cout.fill('0');
        std::cout << std::setw(8) << *((int*)&(reg_fp_list[i])) << " " << std::ends;
        std::cout.setf(std::ios::dec, std::ios::basefield);
        if(i % 4 == 3) std::cout << std::endl;
    }
    return;
}

// startからwidthぶん、4byte単位でメモリの内容を出力
void print_memory(int start, int width){
    for(int i=start; i<start+width; i++){
        std::cout.setf(std::ios::hex, std::ios::basefield);
        std::cout.fill('0');
        std::cout << "mem[" << i << "]: " << memory[i].to_string() << std::endl;
        std::cout.setf(std::ios::dec, std::ios::basefield);
    }
    return;
}

// 終了時の無限ループ命令(jal x0, 0)であるかどうかを判定
bool is_end(Operation op){
    return (op.opcode == 10) && (op.funct == -1) && (op.rs1 = -1) && (op.rs2 == -1) && (op.rd == 0) && (op.imm == 0);
}
