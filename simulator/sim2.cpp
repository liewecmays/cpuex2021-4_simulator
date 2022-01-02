#include <sim2.hpp>
#include <common.hpp>
#include <util.hpp>
#include <fpu.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <optional>
#include <boost/bimap/bimap.hpp>
#include <regex>
#include <iomanip>
#include <boost/program_options.hpp>
#include <chrono>
#include "nameof.hpp"

using ::Otype;
namespace po = boost::program_options;

/* グローバル変数 */
// 内部処理関係
std::vector<Operation> op_list; // 命令のリスト(PC順)
Bit32 reg_list[32]; // 整数レジスタのリスト
Bit32 reg_fp_list[32]; // 浮動小数レジスタのリスト
Bit32 *memory; // メモリ領域

unsigned long long op_count = 0; // 命令のカウント
int mem_size = 100; // メモリサイズ
constexpr unsigned long long max_op_count = 10000000000;

Configuration config; // 各時点の状態

// シミュレーションの制御
bool is_debug = true; // デバッグモード (一時的にデフォルトをtrueに設定)
bool is_bin = false; // バイナリファイルモード
bool is_ieee = false; // IEEE754に従って浮動小数演算を行うモード
bool is_preloading = false; // バッファのデータを予め取得しておくモード
std::string filename; // 処理対象のファイル名
std::string preload_filename; // プリロード対象のファイル名

// 統計・出力関連
unsigned long long op_type_count[op_type_num]; // 各命令の実行数

// 処理用のデータ構造
bimap_t bp_to_id; // ブレークポイントと命令idの対応
bimap_t label_to_id; // ラベルと命令idの対応
bimap_t2 id_to_line; // 命令idと行番号の対応
bimap_t bp_to_id_loaded; // ロードされたもの専用
bimap_t label_to_id_loaded;
bimap_t2 id_to_line_loaded;

// ターミナルへの出力用
std::string head = "\x1b[1m[sim2]\x1b[0m ";


int main(int argc, char *argv[]){
    // コマンドライン引数をパース
    po::options_description opt("./sim2 option");
	opt.add_options()
        ("help,h", "show help")
        ("file,f", po::value<std::string>(), "filename")
        ("debug,d", "debug mode")
        ("bin,b", "binary-input mode")
        ("ieee", "IEEE754 mode")
        ("preload", po::value<std::string>()->implicit_value("contest"), "data preload");
	po::variables_map vm;
    try{
        po::store(po::parse_command_line(argc, argv, opt), vm);
        po::notify(vm);
    }catch(po::error& e){
        std::cout << head_error << e.what() << std::endl;
        std::cout << opt << std::endl;
        std::exit(EXIT_FAILURE);
    }

	if(vm.count("help")){
        std::cout << opt << std::endl;
        std::exit(EXIT_FAILURE);
    }
    if(vm.count("file")){
        filename = vm["file"].as<std::string>();
    }else{
        std::cout << head_error << "filename is required (use -f option)" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    if(vm.count("debug")) is_debug = true;
    if(vm.count("bin")) is_bin = true;
    if(vm.count("ieee")) is_ieee = true;
    if(vm.count("preload")){
        is_preloading = true;
        preload_filename = vm["preload"].as<std::string>();
    };

    // ここからシミュレータの処理開始
    std::cout << head << "simulation start" << std::endl;
    
    // レジスタの初期化
    for(int i=0; i<32; ++i){
        reg_list[i] = Bit32(0);
        reg_fp_list[i] = Bit32(0);
    }

    // メモリ領域の確保
    memory = (Bit32*) calloc(mem_size, sizeof(Bit32));

    // RAMの初期化
    init_ram();

    // バッファのデータのプリロード
    // if(is_preloading){
    //     preload_filename = "./data/" + preload_filename + ".bin";
    //     std::ifstream preload_file(preload_filename, std::ios::in | std::ios::binary);
    //     if(!preload_file){
    //         std::cerr << head_error << "could not open " << preload_filename << std::endl;
    //         std::exit(EXIT_FAILURE);
    //     }
    //     unsigned char c;
    //     while(!preload_file.eof()){
    //         preload_file.read((char*) &c, sizeof(char)); // 8bit取り出す
    //         receive_buffer.push(Bit32(static_cast<int>(c)));
    //     }
    //     std::cout << head << "preloaded data to the receive-buffer from " + preload_filename << std::endl;
    // }

    // ファイルを読む
    std::string input_filename;
    input_filename = "./code/" + filename + (is_bin ? ".bin" : (is_debug ? ".dbg" : ""));
    std::ifstream input_file(input_filename);
    if(!input_file.is_open()){
        std::cerr << head_error << "could not open " << input_filename << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // ファイルの各行をパースしてop_listに追加
    std::string code;
    std::string code_keep; // 空白でない最後の行を保存
    int code_id = 0; // is_skip ? 100 : 0;
    if(!is_bin){
        std::regex regex_empty = std::regex("^\\s*\\r?\\n?$");
        std::regex regex_dbg = std::regex("^\\d{32}@(-?\\d+)$");
        std::regex regex_dbg_label = std::regex("^\\d{32}@(-?\\d+)#(([a-zA-Z_]\\w*(.\\d+)*))$");
        std::regex regex_dbg_bp = std::regex("^\\d{32}@(-?\\d+)!(([a-zA-Z_]\\w*(.\\d+)*))$");
        std::regex regex_dbg_label_bp = std::regex("^\\d{32}@(-?\\d+)#(([a-zA-Z_]\\w*(.\\d+)*))!(([a-zA-Z_]\\w*(.\\d+)*))$");
        std::smatch match;
        while(std::getline(input_file, code)){
            if(std::regex_match(code, regex_empty)){ // 空行は無視
                continue;
            }else{
                op_list.emplace_back(Operation(code));
                
                // ラベル・ブレークポイントの処理
                if(code.size() > 32){
                    if(is_debug){ // デバッグモード
                        if(std::regex_match(code, match, regex_dbg)){
                            id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                        }else if(std::regex_match(code, match, regex_dbg_label)){ // ラベルのみ
                            id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                            label_to_id.insert(bimap_value_t(match[2].str(), code_id));             
                        }else if(std::regex_match(code, match, regex_dbg_bp)){ // ブレークポイントのみ
                            id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                            bp_to_id.insert(bimap_value_t(match[2].str(), code_id));
                        }else if(std::regex_match(code, match, regex_dbg_label_bp)){ // ラベルとブレークポイントの両方
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

                ++code_id;
                code_keep = code;
            }
        }
    }else{
        int read_state = 0;
        unsigned char c;
        int acc = 0;
        while(!input_file.eof()){
            input_file.read((char*) &c, sizeof(char)); // 8bit取り出す
            acc += static_cast<int>(c) << ((3 - read_state) * 8);
            ++read_state;
            if(read_state == 4){
                op_list.emplace_back(Operation(acc));
                acc = 0;
                read_state = 0;
                ++code_id;
            }
        }
    }

    op_list.resize(code_id + 6); // segmentation fault防止のために余裕を持たせる

    // シミュレーションの本体処理
    simulate();

    // 実行結果の情報を出力
    // if(is_info_output || is_detailed_debug) output_info();
}


// シミュレーションの本体処理
void simulate(){
    if(is_debug){ // デバッグモード
        std::string cmd;
        while(true){
            std::cout << "\033[2D# " << std::flush;
            if(!std::getline(std::cin, cmd)) break;
            if(exec_command(cmd)) break;
        }
    }
    // else{ // デバッグなしモード
    //     exec_command("run -t");
    // }

    return;
}

// デバッグモードのコマンドを認識して実行
bool no_info = false; // infoを表示しない(一時的な仕様)
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
        advance_clock(true);
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(do))\\s+(\\d+)\\s*$"))){ // do N
        for(int i=0; i<std::stoi(match[3].str()); ++i){
            advance_clock(false);
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(r|(run))(\\s+(-t))?\\s*$"))){ // run
        
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(init))\\s*$"))){ // init
        config = Configuration();
        op_count = 0; // 総実行命令数を0にする
        for(int i=0; i<32; ++i){ // レジスタをクリア
            reg_list[i] = Bit32(0);
            reg_fp_list[i] = Bit32(0);
        }
        for(int i=0; i<mem_size; ++i){ // メモリをクリア
            memory[i] = Bit32(0);
        }
        std::cout << head_info << "simulation environment is now initialized" << std::endl;
    }else if(std::regex_match(cmd, std::regex("^\\s*(ir|(init run))\\s*$"))){ // init run
        exec_command("init");
        exec_command("run");
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(info))\\s*$"))){ // info
        std::cout << "operations executed: " << op_count << std::endl;
        // if(simulation_end){
        //     std::cout << "next: (no operation left to be simulated)" << std::endl;
        // }else{
        //     std::cout << "next: pc " << pc << " (line " << id_to_line.left.at(id_of_pc(pc)) << ") " << op_list[id_of_pc(pc)].to_string() << std::endl;
        // }
        if(bp_to_id.empty()){
            std::cout << "breakpoints: (no breakpoint found)" << std::endl;
        }else{
            std::cout << "breakpoints:" << std::endl;
            for(auto x : bp_to_id.left) {
                std::cout << "  " << x.first << " (pc " << x.second * 4 << ", line " << id_to_line.left.at(x.second) << ")" << std::endl;
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(p|(print))\\s+reg\\s*$"))){ // print reg
        print_reg();
        print_reg_fp();
    // }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))\\s+buf(\\s+(\\d+))?\\s*$"))){ // print buf
    //     if(receive_buffer.empty()){
    //         std::cout << "receive buffer:" << std::endl;
    //     }else{
    //         int size;
    //         if(match[4].str() == ""){
    //             size = 10;
    //         }else{
    //             size = std::stoi(match[4].str());
    //         }
    //         std::cout << "receive buffer:\n  ";
    //         print_queue(receive_buffer, size);
    //     }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))(\\s+-(d|b|h|f|o))?(\\s+(x|f)(\\d+))+\\s*$"))){ // print (option) reg
        int reg_no;
        Stype st = Stype::t_default;
        char option = match[4].str().front();
        switch(option){
            case 'd': st = Stype::t_dec; break;
            case 'b': st = Stype::t_bin; break;
            case 'h': st = Stype::t_hex; break;
            case 'f': st = Stype::t_float; break;
            case 'o': st = Stype::t_op; break;
            default: break;
        }

        while(std::regex_search(cmd, match, std::regex("(x|f)(\\d+)"))){
            reg_no = std::stoi(match[2].str());
            if(match[1].str() == "x"){ // int
                std::cout << "\x1b[1m%x" << reg_no << "\x1b[0m: " << reg_list[reg_no].to_string(st) << std::endl;
            }else{ // float
                if(st == Stype::t_default) st = Stype::t_float; // デフォルトはfloat
                std::cout << "\x1b[1m%f" << reg_no << "\x1b[0m: " << reg_fp_list[reg_no].to_string(st) << std::endl;
            }
            cmd = match.suffix();
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))(\\s+(-w))?\\s+(m|mem)\\[(\\d+):(\\d+)\\]\\s*$"))){ // print mem[N:M]
        int start = std::stoi(match[6].str());
        int width = std::stoi(match[7].str());
        if(match[4].str() == "-w"){
            print_memory(start, width);
        }else{
            if(start % 4 == 0 && width % 4 == 0){
                print_memory(start/4, width/4);
            }else{
                std::cout << head_error << "memory address should be multiple of 4 (hint: use `print -w m[N:M]` for word addressing)" << std::endl;   
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(s|(set))\\s+(x(\\d+))\\s+(\\d+)\\s*$"))){ // set reg N
        int reg_no = std::stoi(match[4].str());
        int val = std::stoi(match[5].str());
        if(0 < reg_no && reg_no < 31){
            write_reg(reg_no, val);
        }else{
            std::cout << head_error << "invalid argument (integer registers are x0,...,x31)" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(b|(break))\\s+(\\d+)\\s+(([a-zA-Z_]\\w*(.\\d+)*))\\s*$"))){ // break N id (Nはアセンブリコードの行数)
        unsigned int line_no = std::stoi(match[3].str());
        std::string bp = match[4].str();
        if(id_to_line.right.find(line_no) != id_to_line.right.end()){ // 行番号は命令に対応している？
            unsigned int id = id_to_line.right.at(line_no);
            if(bp_to_id.right.find(id) == bp_to_id.right.end()){ // idはまだブレークポイントが付いていない？
                if(label_to_id.right.find(id) == label_to_id.right.end()){ // idにはラベルが付いていない？
                    if(bp_to_id.left.find(bp) == bp_to_id.left.end()){ // そのブレークポイント名は使われていない？
                        if(label_to_id.left.find(bp) == label_to_id.left.end()){ // そのブレークポイント名はラベル名と重複していない？
                            bp_to_id.insert(bimap_value_t(bp, id));
                            if(!no_info){
                                std::cout << head_info << "breakpoint '" << bp << "' is now set to line " << line_no << std::endl;
                            }
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
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(b|(break))\\s+(.+)\\s*$"))){ // break label
        std::string label = match[3].str();
        if(bp_to_id.left.find(label) == bp_to_id.left.end()){
            if(label_to_id.left.find(label) != label_to_id.left.end()){
                int label_id = label_to_id.left.at(label); // 0-indexed
                bp_to_id.insert(bimap_value_t(label, label_id));
                std::cout << head_info << "breakpoint '" << label << "' is now set (at pc " << label_id * 4 << ", line " << id_to_line.left.at(label_id) << ")" << std::endl;
            
            }else{
                std::vector<std::string> matched_labels;
                for(auto x : label_to_id.left){
                    if(x.first.find(label) == 0){ // 先頭一致
                        matched_labels.emplace_back(x.first);
                    }
                }
                unsigned int matched_num = matched_labels.size();
                if(matched_num == 1){
                    std::cout << "one label matched: '" << matched_labels[0] << "'" << std::endl;
                    exec_command("break " + matched_labels[0]);
                }else if(matched_num > 1){
                    std::cout << head_info << "more than one labels matched:" << std::endl;
                    for(auto label : matched_labels){
                        std::cout << "  " << label << " (line " << label_to_id.left.at(label) << ")" << std::endl;
                    }
                }else{
                    std::cout << head_error << "no label matched for '" << label << "'" << std::endl;
                }
            }
        }else{
            std::cout << head_error << "breakpoint '" << label << "' has already been set" << std::endl;  
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(delete))\\s+(([a-zA-Z_]\\w*(.\\d+)*))\\s*$"))){ // delete id
        std::string bp_id = match[3].str();
        if(bp_to_id.left.find(bp_id) != bp_to_id.left.end()){
            bp_to_id.left.erase(bp_id);
            if(!no_info){
                std::cout << head_info << "breakpoint '" << bp_id << "' is now deleted" << std::endl;
            }
        }else{
            std::cout << head_error << "breakpoint '" << bp_id << "' has not been set" << std::endl;  
        }
    }else{
        std::cout << head_error << "invalid command" << std::endl;
    }

    return res;
}

// クロックを1つ分先に進める
void advance_clock(bool verbose){
    Configuration config_next = Configuration(); // configを現在の状態として、次の状態
    config_next.clk = config.clk + 1;

    /* execution */
    // AL
    for(unsigned int i=0; i<2; ++i){
        config.EX.als[i].exec();
        if(!config.EX.als[i].inst.op.is_nop()){
            config_next.wb_req(config.EX.als[i].inst);
        }
    }

    // BR
    config.EX.br.exec();

    // MA
    if(!config.EX.ma.inst.op.is_nop()){
        if(config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle){
            if(config.EX.ma.inst.op.type == o_sw || config.EX.ma.inst.op.type == o_fsw){
                if(config.EX.ma.available()){
                    config.EX.ma.exec();
                    config_next.EX.ma.state = config.EX.ma.state;
                }else{
                    config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Store_data_mem;
                    config_next.EX.ma.inst = config.EX.ma.inst;
                    config_next.EX.ma.cycle_count = 1;
                }
            }else if(config.EX.ma.inst.op.type == o_lw){
                config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_int;
                config_next.EX.ma.inst = config.EX.ma.inst;
                config_next.EX.ma.cycle_count = 1;
            }else if(config.EX.ma.inst.op.type == o_flw){
                config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_fp;
                config_next.EX.ma.inst = config.EX.ma.inst;
                config_next.EX.ma.cycle_count = 1;
            }else{
                config_next.EX.ma.state = config.EX.ma.state;
                config_next.EX.ma.inst = config.EX.ma.inst;
            }
        }else{
            if(config.EX.ma.available()){
                config.EX.ma.exec();
                if(config.EX.ma.inst.op.type == o_lw || config.EX.ma.inst.op.type == o_flw){
                    config_next.wb_req(config.EX.ma.inst);
                }
                config_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Idle;
            }else{
                config_next.EX.ma.state = config.EX.ma.state;
                config_next.EX.ma.inst = config.EX.ma.inst;
                config_next.EX.ma.cycle_count = config.EX.ma.cycle_count + 1;
            }
        }
    }

    // MA (hazard info)
    config.EX.ma.info.wb_addr = config.EX.ma.inst.op.rd; // todo: いらない？
    config.EX.ma.info.is_willing_but_not_ready_int =
        (config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle && (config.EX.ma.inst.op.type == o_lrd || config.EX.ma.inst.op.type == o_lw))
        || (config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_int && !config.EX.ma.available());
    config.EX.ma.info.is_willing_but_not_ready_fp =
        (config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle && config.EX.ma.inst.op.type == o_flw)
        || (config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_fp && !config.EX.ma.available());
    config.EX.ma.info.cannot_accept =
        (config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle && (((config.EX.ma.inst.op.type == o_sw || config.EX.ma.inst.op.type == o_fsw) && !config.EX.ma.available()) || config.EX.ma.inst.op.type == o_lw || config.EX.ma.inst.op.type == o_flw))
        || ((config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Store_data_mem || config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_int || config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_fp) && !config.EX.ma.available());

    // mFP
    if(!config.EX.mfp.inst.op.is_nop()){
        if(config.EX.mfp.state == Configuration::EX_stage::EX_mfp::State_mfp::Waiting){
            if(config.EX.mfp.available()){
                config.EX.mfp.exec();
                config_next.wb_req(config.EX.mfp.inst);
                config_next.EX.mfp.state = config.EX.mfp.state;
            }else{
                std::cout << "here" << std::endl;
                config_next.EX.mfp.state = Configuration::EX_stage::EX_mfp::State_mfp::Processing;
                config_next.EX.mfp.inst = config.EX.mfp.inst;
                config_next.EX.mfp.cycle_count = 1;
            }
        }else{
            if(config.EX.mfp.available()){
                config.EX.mfp.exec();
                config_next.wb_req(config.EX.mfp.inst);
                config_next.EX.mfp.state = Configuration::EX_stage::EX_mfp::State_mfp::Waiting;
            }else{
                config_next.EX.mfp.state = config.EX.mfp.state;
                config_next.EX.mfp.inst = config.EX.mfp.inst;
                config_next.EX.mfp.cycle_count = config.EX.mfp.cycle_count + 1;
            }
        }
    }

    // mFP (hazard info)
    config.EX.mfp.info.wb_addr = config.EX.mfp.inst.op.rd;
    config.EX.mfp.info.is_willing_but_not_ready = (config_next.EX.mfp.state == Configuration::EX_stage::EX_mfp::State_mfp::Processing);
    config.EX.mfp.info.cannot_accept = (config_next.EX.mfp.state == Configuration::EX_stage::EX_mfp::State_mfp::Processing);

    // pFP
    for(unsigned int i=1; i<pipelined_fpu_stage_num; ++i){
        config_next.EX.pfp.inst[i] = config.EX.pfp.inst[i-1];
    }
    config.EX.pfp.exec();
    config_next.wb_req(config.EX.pfp.inst[pipelined_fpu_stage_num-1]);

    // pFP (hazard info)
    for(unsigned int i=0; i<pipelined_fpu_stage_num-1; ++i){
        config.EX.pfp.info.wb_addr[i] = config.EX.pfp.inst[i].op.rd;
        config.EX.pfp.info.wb_en[i] = config.EX.pfp.inst[i].op.use_pipelined_fpu();       
    }


    /* instruction fetch/decode */
    // dispatch?
    config.ID.hazard_type[0] = config.inter_hazard_detector(0) || config.iwp_hazard_detector(0);
    if(config.ID.is_not_dispatched(0)){
        config.ID.hazard_type[1] = Configuration::Hazard_type::Trivial;
    }else{
        config.ID.hazard_type[1] = config.intra_hazard_detector() || config.inter_hazard_detector(1) || config.iwp_hazard_detector(1);
    }

    // pc manager
    if(config.EX.br.branch_addr.has_value()){
        config.IF.pc[0] = config.EX.br.branch_addr.value();
        config.IF.pc[1] = config.EX.br.branch_addr.value() + 4;
    }else if(config.ID.is_not_dispatched(0)){
        config.IF.pc = config.ID.pc;
    }else if(config.ID.is_not_dispatched(1)){
        config.IF.pc[0] = config.ID.pc[0] + 4;
        config.IF.pc[1] = config.ID.pc[1] + 4;
    }else{
        config.IF.pc[0] = config.ID.pc[0] + 8;
        config.IF.pc[1] = config.ID.pc[1] + 8;
    }

    // instruction fetch
    config_next.ID.pc = config.IF.pc;
    config_next.ID.op[0] = op_list[id_of_pc(config.IF.pc[0])];
    config_next.ID.op[1] = op_list[id_of_pc(config.IF.pc[1])];

    // distribution + reg fetch
    for(unsigned int i=0; i<2; ++i){
        if(config.EX.br.branch_addr.has_value() || config.ID.is_not_dispatched(i)) continue; // rst from br/id
        switch(config.ID.op[i].type){
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
                config_next.EX.als[i].inst.op = config.ID.op[i];
                config_next.EX.als[i].inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                config_next.EX.als[i].inst.rs2_v = read_reg_32(config.ID.op[i].rs2);
                config_next.EX.als[i].inst.pc = config.ID.pc[i];
                break;
            case o_fmvfi:
                config_next.EX.als[i].inst.op = config.ID.op[i];
                config_next.EX.als[i].inst.rs1_v = read_reg_fp_32(config.ID.op[i].rs1);
                config_next.EX.als[i].inst.pc = config.ID.pc[i];
                break;
            // BR (conditional)
            case o_beq:
            case o_blt:
            case o_fbeq:
            case o_fblt:
                config_next.EX.br.inst.op = config.ID.op[i];
                config_next.EX.br.inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                config_next.EX.br.inst.rs2_v = read_reg_32(config.ID.op[i].rs2);
                config_next.EX.br.inst.pc = config.ID.pc[i];
                break;
            // BR (unconditional)
            case o_jal:
            case o_jalr:
                // ALとBRの両方にdistribute
                config_next.EX.als[i].inst.op = config.ID.op[i];
                config_next.EX.als[i].inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                config_next.EX.als[i].inst.pc = config.ID.pc[i];
                config_next.EX.br.inst.op = config.ID.op[i];
                config_next.EX.br.inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                config_next.EX.br.inst.pc = config.ID.pc[i];
                break;
            // MA
            case o_sw:
            case o_lw:
            case o_flw:
                config_next.EX.ma.inst.op = config.ID.op[i];
                config_next.EX.ma.inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                config_next.EX.ma.inst.rs2_v = read_reg_32(config.ID.op[i].rs2);
                config_next.EX.ma.inst.pc = config.ID.pc[i];
                break;
            case o_fsw:
                config_next.EX.ma.inst.op = config.ID.op[i];
                config_next.EX.ma.inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                config_next.EX.ma.inst.rs2_v = read_reg_fp_32(config.ID.op[i].rs2);
                config_next.EX.ma.inst.pc = config.ID.pc[i];
                break;
            // mFP
            case o_fdiv:
            case o_fsqrt:
            case o_fcvtif:
            case o_fcvtfi:
            case o_fmvff:
                config_next.EX.mfp.inst.op = config.ID.op[i];
                config_next.EX.mfp.inst.rs1_v = read_reg_fp_32(config.ID.op[i].rs1);
                config_next.EX.mfp.inst.rs2_v = read_reg_fp_32(config.ID.op[i].rs2);
                config_next.EX.mfp.inst.pc = config.ID.pc[i];
                break;
            case o_fmvif:
                config_next.EX.mfp.inst.op = config.ID.op[i];
                config_next.EX.mfp.inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                config_next.EX.mfp.inst.pc = config.ID.pc[i];
                break;
            // pFP
            case o_fadd:
            case o_fsub:
            case o_fmul:
                config_next.EX.pfp.inst[0].op = config.ID.op[i];
                config_next.EX.pfp.inst[0].rs1_v = read_reg_fp_32(config.ID.op[i].rs1);
                config_next.EX.pfp.inst[0].rs2_v = read_reg_fp_32(config.ID.op[i].rs2);
                config_next.EX.pfp.inst[0].pc = config.ID.pc[i];
                break;
            case o_nop: break;
            default: std::exit(EXIT_FAILURE);
        }
    }

    /* print */
    if(verbose){
        std::cout << "clk: " << config.clk << std::endl;

        // IF
        std::cout << "\x1b[1m[IF]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            std::cout << (i==0 ? " " : "     ") << "if[" << i << "] : pc=" << config.IF.pc[i] << std::endl;
        }

        // ID
        std::cout << "\x1b[1m[ID]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            std::cout << (i==0 ? " " : "     ") << "id[" << i << "] : " << config.ID.op[i].to_string() << " (pc=" << config.ID.pc[i] << ")" << (config.ID.is_not_dispatched(i) ? ("\x1b[1m\x1b[31m -> not dispatched\x1b[0m [" + std::string(NAMEOF_ENUM(config.ID.hazard_type[i])) + "]") : "\x1b[1m -> dispatched\x1b[0m") << std::endl;
        }

        // EX
        std::cout << "\x1b[1m[EX]\x1b[0m";
        
        // EX_al
        for(unsigned int i=0; i<2; ++i){
            if(!config.EX.als[i].inst.op.is_nop()){
                std::cout << (i==0 ? " " : "     ") << "al" << i << "   : " << config.EX.als[i].inst.op.to_string() << " (pc=" << config.EX.als[i].inst.pc << ")" << std::endl;
            }else{
                std::cout << (i==0 ? " " : "     ") << "al" << i << "   :" << std::endl;
            }
        }

        // EX_br
        if(!config.EX.br.inst.op.is_nop()){
            std::cout << "     br    : " << config.EX.br.inst.op.to_string() << " (pc=" << config.EX.br.inst.pc << ")" << (config.EX.br.branch_addr.has_value() ? "\x1b[1m -> taken\x1b[0m" : "\x1b[1m -> untaken\x1b[0m") << std::endl;
        }else{
            std::cout << "     br    :" << std::endl;
        }

        // EX_ma
        if(!config.EX.ma.inst.op.is_nop()){
            std::cout << "     ma    : " << config.EX.ma.inst.op.to_string() << " (pc=" << config.EX.ma.inst.pc << ") [state: " << NAMEOF_ENUM(config.EX.ma.state) << (config.EX.ma.state != Configuration::EX_stage::EX_ma::State_ma::Idle ? (", cycle: " + std::to_string(config.EX.ma.cycle_count)) : "") << "]" << (config.EX.ma.available() ? "\x1b[1m\x1b[32m -> available\x1b[0m" : "") << std::endl;
        }else{
            std::cout << "     ma    :" << std::endl;
        }

        // EX_mfp
        if(!config.EX.mfp.inst.op.is_nop()){
            std::cout << "     mfp   : " << config.EX.mfp.inst.op.to_string() << " (pc=" << config.EX.mfp.inst.pc << ") [state: " << NAMEOF_ENUM(config.EX.mfp.state) << (config.EX.mfp.state != Configuration::EX_stage::EX_mfp::State_mfp::Waiting ? (", cycle: " + std::to_string(config.EX.mfp.cycle_count)) : "") << "]" << (config.EX.mfp.available() ? "\x1b[1m\x1b[32m -> available\x1b[0m" : "") << std::endl;
        }else{
            std::cout << "     mfp   :" << std::endl;
        }

        // EX_pfp
        for(unsigned int i=0; i<pipelined_fpu_stage_num; ++i){
            if(!config.EX.pfp.inst[i].op.is_nop()){
                std::cout << "     pfp[" << i << "]: " << config.EX.pfp.inst[i].op.to_string() << " (pc=" << config.EX.pfp.inst[i].pc << ")" << std::endl;
            }else{
                std::cout << "     pfp[" << i << "]:" << std::endl;
            }
        }

        // WB
        std::cout << "\x1b[1m[WB]\x1b[0m";
        for(unsigned int i=0; i<2; ++i){
            if(config.WB.inst_int[i].has_value()){
                std::cout << (i==0 ? " " : "     ") << "int[" << i << "]: " << config.WB.inst_int[i].value().op.to_string() << " (pc=" << config.WB.inst_int[i].value().pc << ")" << std::endl;
            }else{
                std::cout << (i==0 ? " " : "     ") << "int[" << i << "]:" << std::endl;
            }
        }
        for(unsigned int i=0; i<2; ++i){
            if(config.WB.inst_fp[i].has_value()){
                std::cout << "     fp[" << i << "] : " << config.WB.inst_fp[i].value().op.to_string() << " (pc=" << config.WB.inst_fp[i].value().pc << ")" << std::endl;
            }else{
                std::cout << "     fp[" << i << "] :" << std::endl;
            }
        }
    }

    /* update */
    config = config_next;
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
            write_reg(this->inst.op.rd, this->inst.rs1_v.i + this->inst.rs2_v.i);
            ++op_type_count[o_add];
            return;
        case o_sub:
            write_reg(this->inst.op.rd, this->inst.rs1_v.i - this->inst.rs2_v.i);
            ++op_type_count[o_sub];
            return;
        case o_sll:
            write_reg(this->inst.op.rd, this->inst.rs1_v.i << this->inst.rs2_v.i);
            ++op_type_count[o_sll];
            return;
        case o_srl:
            write_reg(this->inst.op.rd, static_cast<unsigned int>(this->inst.rs1_v.i) >> this->inst.rs2_v.i);
            ++op_type_count[o_srl];
            return;
        case o_sra:
            write_reg(this->inst.op.rd, this->inst.rs1_v.i >> this->inst.rs2_v.i); // todo: 処理系依存
            ++op_type_count[o_sra];
            return;
        case o_and:
            write_reg(this->inst.op.rd, this->inst.rs1_v.i & this->inst.rs2_v.i);
            ++op_type_count[o_and];
            return;
        // op_imm
        case o_addi:
            write_reg(this->inst.op.rd, this->inst.rs1_v.i + this->inst.op.imm);
            ++op_type_count[o_addi];
            return;
        case o_slli:
            write_reg(this->inst.op.rd, this->inst.rs1_v.i << this->inst.op.imm);
            ++op_type_count[o_slli];
            return;
        case o_srli:
            write_reg(this->inst.op.rd, static_cast<unsigned int>(this->inst.rs1_v.i) >> this->inst.op.imm);
            ++op_type_count[o_srli];
            return;
        case o_srai:
            write_reg(this->inst.op.rd, this->inst.rs1_v.i >> this->inst.op.imm); // todo: 処理系依存
            ++op_type_count[o_srai];
            return;
        case o_andi:
            write_reg(this->inst.op.rd, this->inst.rs1_v.i & this->inst.op.imm);
            ++op_type_count[o_andi];
            return;
        // lui
        case o_lui:
            write_reg(this->inst.op.rd, this->inst.op.imm << 12);
            ++op_type_count[o_lui];
            return;
        // ftoi
        case o_fmvfi:
            write_reg_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[o_fmvfi];
            return;
        // jalr (pass through)
        case o_jalr:
            write_reg(this->inst.op.rd, this->inst.pc + 4);
            return;
        // jal (pass through)
        case o_jal:
            write_reg(this->inst.op.rd, this->inst.pc + 4);
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
                write_reg_32(this->inst.op.rd, read_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4));
            }else{
                exit_with_output("address of load operation should be multiple of 4 [lw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[o_lw];
            return;
        case o_flw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                write_reg_fp_32(this->inst.op.rd, read_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4));
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
    return this->cycle_count == 2; // 仮の値
}

void Configuration::EX_stage::EX_mfp::exec(){
    switch(this->inst.op.type){
        // op_fp
        case o_fdiv:
            if(is_ieee){
                write_reg_fp(this->inst.op.rd, this->inst.rs1_v.f / this->inst.rs2_v.f);
            }else{
                write_reg_fp_32(this->inst.op.rd, fdiv(this->inst.rs1_v, this->inst.rs2_v));
            }
            ++op_type_count[o_fdiv];
            return;
        case o_fsqrt:
            if(is_ieee){
                write_reg_fp(this->inst.op.rd, std::sqrt(this->inst.rs1_v.f));
            }else{
                write_reg_fp_32(this->inst.op.rd, fsqrt(this->inst.rs1_v));
            }
            ++op_type_count[o_fsqrt];
            return;
        case o_fcvtif:
            if(is_ieee){
                write_reg_fp(this->inst.op.rd, static_cast<float>(this->inst.rs1_v.i));
            }else{
                write_reg_fp_32(this->inst.op.rd, itof(this->inst.rs1_v));
            }
            ++op_type_count[o_fcvtif];
            return;
        case o_fcvtfi:
            if(is_ieee){
                write_reg_fp(this->inst.op.rd, static_cast<int>(std::nearbyint(this->inst.rs1_v.f)));
            }else{
                write_reg_fp_32(this->inst.op.rd, ftoi(this->inst.rs1_v));
            }
            ++op_type_count[o_fcvtfi];
            return;
        case o_fmvff:
            write_reg_fp_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[o_fmvff];
            return;
        // itof
        case o_fmvif:
            write_reg_fp_32(this->inst.op.rd, this->inst.rs1_v);
            ++op_type_count[o_fmvif];
            return;
        case o_nop: return;
        default:
            exit_with_output("invalid operation for mFP (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

bool Configuration::EX_stage::EX_mfp::available(){
    return this->cycle_count == 2; // 仮の値
}

void Configuration::EX_stage::EX_pfp::exec(){
    Instruction inst = this->inst[pipelined_fpu_stage_num-1];
    switch(inst.op.type){
        case o_fadd:
            if(is_ieee){
                write_reg_fp(inst.op.rd, inst.rs1_v.f + inst.rs2_v.f);
            }else{
                write_reg_fp_32(inst.op.rd, fadd(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fadd];
            return;
        case o_fsub:
            if(is_ieee){
                write_reg_fp(inst.op.rd, inst.rs1_v.f - inst.rs2_v.f);
            }else{
                write_reg_fp_32(inst.op.rd, fsub(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fsub];
            return;
        case o_fmul:
            if(is_ieee){
                write_reg_fp(inst.op.rd, inst.rs1_v.f * inst.rs2_v.f);
            }else{
                write_reg_fp_32(inst.op.rd, fmul(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[o_fmul];
            return;
        case o_nop: return;
        default:
            exit_with_output("invalid operation for pFP (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(inst.pc)))) : "") + ")");
    }
}

// void exec_inst(Instruction inst){
//     switch(inst.op.type){
        // case o_si:
        //     if((inst.rs1_v + op.imm) % 4 == 0){
        //         op_list[(inst.rs1_v + op.imm) / 4] = Operation(inst.rs2_v);
        //     }else{
        //         exit_with_output("address of store operation should be multiple of 4 [si] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
        //     }
        //     ++op_type_count[o_si];
        //     pc += 4;
        //     return;
        // case o_std:
        //     send_buffer.push(inst.rs2_v);
        //     ++op_type_count[o_std];
        //     pc += 4;
        //     return;
        // case o_lre:
        //     write_reg(op.rd, receive_buffer.empty() ? 1 : 0);
        //     pc += 4;
        //     ++op_type_count[o_lre];
        //     return;
        // case o_lrd:
        //     if(!receive_buffer.empty()){
        //         write_reg(op.rd, receive_buffer.front().i);
        //         receive_buffer.pop();
        //     }else{
        //         exit_with_output("receive buffer is empty [lrd] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
        //     }
        //     ++op_type_count[o_lrd];
        //     pc += 4;
        //     return;
        // case o_ltf:
        //     write_reg(op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
        //     ++op_type_count[o_ltf];
        //     pc += 4;
        //     return;
        // default:
        //     exit_with_output("error in executing the code (at pc " + std::to_string(this->in) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
//     }
// }

// PCから命令IDへの変換(4の倍数になっていない場合エラー)
inline unsigned int id_of_pc(int n){
    if(n % 4 == 0){
        return n / 4;
    }else{
        exit_with_output("error with program counter: pc = " + std::to_string(n));
        return 0; // 実行されない
    }
}

// 整数レジスタから読む
inline int read_reg(unsigned int i){
    return i == 0 ? 0 : reg_list[i].i;
}
// 整数レジスタから読む(Bit32で)
inline Bit32 read_reg_32(unsigned int i){
    return i == 0 ? 0 : reg_list[i];
}

// 整数レジスタに書き込む
inline void write_reg(unsigned int i, int v){
    if (i != 0) reg_list[i] = Bit32(v);
    // if(is_raytracing && i == 2 && v > max_x2) max_x2 = v;
    return;
}
// 整数レジスタに書き込む(Bit32で)
inline void write_reg_32(unsigned int i, Bit32 v){
    if (i != 0) reg_list[i] = v;
    // if(is_raytracing && i == 2 && v.i > max_x2) max_x2 = v.i;
    return;
}

// 浮動小数点数レジスタから読む
inline float read_reg_fp(unsigned int i){
    return i == 0 ? 0 : reg_fp_list[i].f;
}
// 浮動小数点数レジスタから読む(Bit32で)
inline Bit32 read_reg_fp_32(unsigned int i){
    return i == 0 ? Bit32(0) : reg_fp_list[i];
}

// 浮動小数点数レジスタに書き込む
inline void write_reg_fp(unsigned int i, float v){
    if (i != 0) reg_fp_list[i] = Bit32(v);
    return;
}
inline void write_reg_fp(unsigned int i, int v){
    if (i != 0) reg_fp_list[i] = Bit32(v);
    return;
}
// 浮動小数点数レジスタに書き込む(Bit32のまま)
inline void write_reg_fp_32(unsigned int i, Bit32 v){
    if (i != 0) reg_fp_list[i] = v;
    return;
}

inline Bit32 read_memory(int w){
    return memory[w];
}

inline void write_memory(int w, Bit32 v){
    memory[w] = v;
}

// 整数レジスタの内容を表示
void print_reg(){
    for(int i=0; i<32; ++i){
        std::cout << "\x1b[1mx" << i << "\x1b[0m:" << std::ends;
        if(i < 10) std::cout << " " << std::ends;
        std::cout.setf(std::ios::hex, std::ios::basefield);
        std::cout.fill('0');
        std::cout << reg_list[i].to_string() << " " << std::ends;
        std::cout.setf(std::ios::dec, std::ios::basefield);
        if(i % 4 == 3) std::cout << std::endl;
    }
    return;
}

// 浮動小数点数レジスタの内容を表示
void print_reg_fp(){
    for(int i=0; i<32; ++i){
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
    for(int i=start; i<start+width; ++i){
        std::cout.setf(std::ios::hex, std::ios::basefield);
        std::cout.fill('0');
        std::cout << "mem[" << i << "]: " << memory[i].to_string() << std::endl;
        std::cout.setf(std::ios::dec, std::ios::basefield);
    }
    return;
}

// 終了時の無限ループ命令(jal x0, 0)であるかどうかを判定
inline bool is_end(Operation op){
    return (op.type == o_jal) && (op.rd == 0) && (op.imm == 0);
}

// 実効情報を表示したうえで異常終了
void exit_with_output(std::string msg){
    std::cout << head_error << msg << std::endl;
    // if(is_info_output){
    //     std::cout << head << "outputting execution info until now" << std::endl;
    //     output_info();
    // }else if(is_debug){
    //     std::cout << head << "execution info until now:" << std::endl;
    //     exec_command("info");
    // }
    std::cout << head << "abnormal end" << std::endl;
    std::quick_exit(EXIT_FAILURE);
}
