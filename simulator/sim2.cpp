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

unsigned long long clk_count = 0; // クロックのカウンタ
unsigned long long op_count = 0; // 命令のカウント
int mem_size = 100; // メモリサイズ
constexpr unsigned long long max_op_count = 10000000000;

Configuration config; // 各時点の状態

// シミュレーションの制御
bool is_debug = true; // デバッグモード (一時的にデフォルトをtrueに設定)
bool is_bin = false; // バイナリファイルモード
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
        advance_clock();
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
    //         std::cout << "receive buffer: (empty)" << std::endl;
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

bool at_power_on = true;
// クロックを1つ分先に進める
void advance_clock(){
    Configuration cfg_next = Configuration(); // configを現在の状態として、次の状態

    std::cout << "clock=" << clk_count << std::endl;

    /* execution */
    // AL
    for(unsigned int i=0; i<2; ++i){
        config.EX.als[i].exec();
        if(!config.EX.als[i].inst.op.is_nop()){
            cfg_next.wb_req(config.EX.als[i].inst);
        }
    }

    // BR
    config.EX.br.exec();

    // MA
    if(!config.EX.ma.inst.op.is_nop()){
        if(config.EX.ma.state == Configuration::EX_stage::EX_ma::State_ma::Idle){
            if(config.EX.ma.inst.op.type == o_sw || config.EX.ma.inst.op.type == o_fsw){
                if(config.EX.ma.available()){
                    std::cout << "available!" << std::endl;
                    cfg_next.EX.ma.state = config.EX.ma.state;
                }else{
                    cfg_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Store_data_mem;
                    cfg_next.EX.ma.inst = config.EX.ma.inst;
                    cfg_next.EX.ma.cycle_count = 1;
                }
            }else if(config.EX.ma.inst.op.type == o_lw){
                cfg_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_int;
                cfg_next.EX.ma.inst = config.EX.ma.inst;
                cfg_next.EX.ma.cycle_count = 1;
            }else if(config.EX.ma.inst.op.type == o_flw){
                cfg_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Load_data_mem_fp;
                cfg_next.EX.ma.inst = config.EX.ma.inst;
                cfg_next.EX.ma.cycle_count = 1;
            }else{
                cfg_next.EX.ma.state = config.EX.ma.state;
                cfg_next.EX.ma.inst = config.EX.ma.inst;
            }
        }else{
            if(config.EX.ma.available()){
                config.EX.ma.exec();
                std::cout << "MA exec" << std::endl;
                if(config.EX.ma.inst.op.type == o_lw || config.EX.ma.inst.op.type == o_flw){
                    cfg_next.wb_req(config.EX.ma.inst);
                }
                cfg_next.EX.ma.state = Configuration::EX_stage::EX_ma::State_ma::Idle;
            }else{
                cfg_next.EX.ma.state = config.EX.ma.state;
                cfg_next.EX.ma.inst = config.EX.ma.inst;
                cfg_next.EX.ma.cycle_count = config.EX.ma.cycle_count + 1;
            }
        }
    }

    // MA (hazard)
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
    // config.EX.mfp.exec();

    // pFP
    // config.EX.pfp.exec();


    /* instruction fetch/decode */
    // dispatch?
    config.ID.is_not_dispatched[0] = config.inter_hazard_detector(0) || config.iwp_hazard_detector(0);
    config.ID.is_not_dispatched[1] = config.ID.is_not_dispatched[0] || config.intra_hazard_detector() || config.inter_hazard_detector(1) || config.iwp_hazard_detector(1);

    // pc manager
    if(config.EX.br.branch_addr.has_value()){
        config.IF.pc[0] = config.EX.br.branch_addr.value();
        config.IF.pc[1] = config.EX.br.branch_addr.value() + 4;
    }else if(config.ID.is_not_dispatched[0]){
        config.IF.pc = config.ID.pc;
    }else if(config.ID.is_not_dispatched[1]){
        config.IF.pc[0] = config.ID.pc[0] + 4;
        config.IF.pc[1] = config.ID.pc[1] + 4;
    }else{
        config.IF.pc[0] = config.ID.pc[0] + 8;
        config.IF.pc[1] = config.ID.pc[1] + 8;
    }

    // instruction fetch
    cfg_next.ID.pc = config.IF.pc;
    cfg_next.ID.op[0] = op_list[id_of_pc(config.IF.pc[0])];
    cfg_next.ID.op[1] = op_list[id_of_pc(config.IF.pc[1])];

    // distribution + reg fetch
    for(unsigned int i=0; i<2; ++i){
        if(config.EX.br.branch_addr.has_value()) continue; // rst from br
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
            case o_fmvfi:
                if(!config.ID.is_not_dispatched[i]){
                    cfg_next.EX.als[i].inst.op = config.ID.op[i];
                    cfg_next.EX.als[i].inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                    cfg_next.EX.als[i].inst.rs2_v = read_reg_32(config.ID.op[i].rs2);
                    cfg_next.EX.als[i].inst.pc = config.ID.pc[i];
                }
                break;
            // BR
            case o_beq:
            case o_blt:
            case o_fbeq:
            case o_fblt:
                if(config.ID.op[0].branch_conditionally_or_unconditionally() ? !config.ID.is_not_dispatched[0] : !config.ID.is_not_dispatched[1]){
                    cfg_next.EX.br.inst.op = config.ID.op[i];
                    cfg_next.EX.br.inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                    cfg_next.EX.br.inst.rs2_v = read_reg_32(config.ID.op[i].rs2);
                    cfg_next.EX.br.inst.pc = config.ID.pc[i];
                }
                break;
            // BR
            case o_jal:
            case o_jalr:
                // ALとBRの両方にdistribute
                if(!config.ID.is_not_dispatched[i]){
                    cfg_next.EX.als[i].inst.op = config.ID.op[i];
                    cfg_next.EX.als[i].inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                    cfg_next.EX.als[i].inst.rs2_v = read_reg_32(config.ID.op[i].rs2);
                    cfg_next.EX.als[i].inst.pc = config.ID.pc[i];
                }
                if(!config.ID.is_not_dispatched[i]){ // todo: これでよい？
                    cfg_next.EX.br.inst.op = config.ID.op[i];
                    cfg_next.EX.br.inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                    cfg_next.EX.br.inst.rs2_v = read_reg_32(config.ID.op[i].rs2);
                    cfg_next.EX.br.inst.pc = config.ID.pc[i];
                }
                break;
            // MA (int)
            case o_sw:
            case o_lw:
                if(!config.ID.is_not_dispatched[i]){
                    cfg_next.EX.ma.inst.op = config.ID.op[i];
                    cfg_next.EX.ma.inst.rs1_v = read_reg_32(config.ID.op[i].rs1);
                    cfg_next.EX.ma.inst.rs2_v = read_reg_32(config.ID.op[i].rs2);
                    cfg_next.EX.ma.inst.pc = config.ID.pc[i];
                }
                break;
            // MA (float)
            case o_fsw:
            case o_flw:
                if(!config.ID.is_not_dispatched[i]){
                    cfg_next.EX.ma.inst.op = config.ID.op[i];
                    cfg_next.EX.ma.inst.rs1_v = read_reg_fp_32(config.ID.op[i].rs1);
                    cfg_next.EX.ma.inst.rs2_v = read_reg_fp_32(config.ID.op[i].rs2);
                    cfg_next.EX.ma.inst.pc = config.ID.pc[i];
                }
                break;
            case o_nop: break;
            // todo: その他の場合
            default: std::exit(EXIT_FAILURE);
        }
    }


    std::cout << "IF:" << std::endl;
    for(unsigned int i=0; i<2; ++i){
        std::cout << "  if" << i << ": pc=" << config.IF.pc[i] << std::endl;
    }

    std::cout << "ID:" << std::endl;
    for(unsigned int i=0; i<2; ++i){
        std::cout << "  id" << i << ": pc=" << config.ID.pc[i] << ", " << config.ID.op[i].to_string() << " -> " << (config.ID.is_not_dispatched[i] ? "not dispatched" : "dispatched") << std::endl;
    }

    std::cout << "EX:" << std::endl;
    for(unsigned int i=0; i<2; ++i){
        if(!config.EX.als[i].inst.op.is_nop()){
            std::cout << "  al" << i << ": pc=" << config.EX.als[i].inst.pc << ", " << config.EX.als[i].inst.op.to_string() << std::endl;
        }else{
            std::cout << "  al" << i << ": (empty)" << std::endl;
        }
    }
    if(!config.EX.br.inst.op.is_nop()){
        std::cout << "  br : pc=" << config.EX.br.inst.pc << ", " << config.EX.br.inst.op.to_string() << " -> " << (config.EX.br.branch_addr.has_value() ? "taken" : "untaken") << std::endl;
    }else{
        std::cout << "  br : (empty)" << std::endl;
    }
    if(!config.EX.ma.inst.op.is_nop()){
        std::cout << "  ma : pc=" << config.EX.ma.inst.pc << ", " << config.EX.ma.inst.op.to_string() << " (state = " << NAMEOF_ENUM(config.EX.ma.state) << (config.EX.ma.state != Configuration::EX_stage::EX_ma::State_ma::Idle ? (", cycle = " + std::to_string(config.EX.ma.cycle_count)) : "") << ")" << std::endl;
    }else{
        std::cout << "  ma : (empty)" << std::endl;
    }

    std::cout << "WB:" << std::endl;
    for(unsigned int i=0; i<2; ++i){
        if(config.WB.inst[i].has_value()){
            std::cout << "  wb" << i << ": pc=" << config.WB.inst[i].value().pc << ", " << config.WB.inst[i].value().op.to_string() << std::endl;
        }else{
            std::cout << "  wb" << i << ": (empty)" << std::endl;
        }
    }

    // update
    config = cfg_next;
    ++clk_count;
}

// 同時発行される命令の間のハザード検出
bool Configuration::intra_hazard_detector(){
    // RAW hazards
    bool rd_to_rs1 =
        ((this->ID.op[0].use_rd_int() && this->ID.op[1].use_rs1_int())
        || (this->ID.op[0].use_rd_fp() && this->ID.op[1].use_rs1_fp()))
        && this->ID.op[0].rd == this->ID.op[1].rs1;
    bool rd_to_rs2 =
        ((this->ID.op[0].use_rd_int() && this->ID.op[1].use_rs2_int())
        || (this->ID.op[0].use_rd_fp() && this->ID.op[1].use_rs2_fp()))
        && this->ID.op[0].rd == this->ID.op[1].rs2;
    bool raw_hazard = rd_to_rs1 || rd_to_rs2;

    // WAW hazards
    bool waw_hazard =
        ((this->ID.op[0].use_rd_int() && this->ID.op[1].use_rd_int())
        || (this->ID.op[0].use_rd_fp() && this->ID.op[1].use_rd_fp()))
        && this->ID.op[0].rd == this->ID.op[1].rd;

    // control hazards
    bool control_hazard = this->ID.op[0].branch_conditionally_or_unconditionally();

    // structural hazards
    bool structural_hazard =
        (this->ID.op[0].use_mem() && this->ID.op[1].use_mem()) // 同時にメモリ
        || (this->ID.op[0].use_multicycle_fpu() && this->ID.op[1].use_multicycle_fpu()) // 同時にFPU(マルチサイクル)
        || (this->ID.op[0].use_pipelined_fpu() && this->ID.op[1].use_pipelined_fpu()); // 同時にFPU(パイプライン)

    return raw_hazard || waw_hazard || control_hazard || structural_hazard;
}

// 同時発行されない命令間のハザード検出
bool Configuration::inter_hazard_detector(unsigned int i){ // i = 0,1
    // RAW hazards
    bool ma_to_rs1 =
        ((this->EX.ma.info.is_willing_but_not_ready_int && this->ID.op[i].use_rs1_int())
        || (this->EX.ma.info.is_willing_but_not_ready_fp && this->ID.op[i].use_rs1_fp()))
        && this->EX.ma.info.wb_addr == this->ID.op[i].rs1;
    bool ma_to_rs2 =
        ((this->EX.ma.info.is_willing_but_not_ready_int && this->ID.op[i].use_rs2_int())
        || (this->EX.ma.info.is_willing_but_not_ready_fp && this->ID.op[i].use_rs2_fp()))
        && this->EX.ma.info.wb_addr == this->ID.op[i].rs2;
    bool mfp_to_rs1 = this->EX.mfp.info.is_willing_but_not_ready && this->ID.op[i].use_rs1_fp() && (this->EX.mfp.info.wb_addr == this->ID.op[i].rs1);
    bool mfp_to_rs2 = this->EX.mfp.info.is_willing_but_not_ready && this->ID.op[i].use_rs2_fp() && (this->EX.mfp.info.wb_addr == this->ID.op[i].rs2);
    bool pfp_to_rs1 = false;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.info.wb_en[j] && this->ID.op[i].use_rs1_fp() && (this->EX.pfp.info.wb_addr[j] == this->ID.op[i].rs1)){
            pfp_to_rs1 = true;
            break;
        }
    }
    bool pfp_to_rs2 = false;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.info.wb_en[j] && this->ID.op[i].use_rs2_fp() && (this->EX.pfp.info.wb_addr[j] == this->ID.op[i].rs2)){
            pfp_to_rs2 = true;
            break;
        }
    }
    bool raw_hazard = ma_to_rs1 || ma_to_rs2 || mfp_to_rs1 || mfp_to_rs2 || pfp_to_rs1 || pfp_to_rs2;

    // WAW hazards
    bool ma_to_rd =
        ((this->EX.ma.info.is_willing_but_not_ready_int && this->ID.op[i].use_rd_int())
        || (this->EX.ma.info.is_willing_but_not_ready_fp && this->ID.op[i].use_rd_fp()))
        && this->EX.ma.info.wb_addr == this->ID.op[i].rd;
    bool mfp_to_rd = this->EX.mfp.info.is_willing_but_not_ready && this->ID.op[i].use_rd_fp() && (this->EX.mfp.info.wb_addr == this->ID.op[i].rd);
    bool pfp_to_rd = false;
    for(unsigned int j=0; j<pipelined_fpu_stage_num-1; ++j){
        if(this->EX.pfp.info.wb_en[j] && this->ID.op[i].use_rd_fp() && (this->EX.pfp.info.wb_addr[j] == this->ID.op[i].rd)){
            pfp_to_rd = true;
            break;
        }
    }
    bool waw_hazard = ma_to_rd || mfp_to_rd || pfp_to_rd;

    // structural hazards
    bool structural_hazard =
        (this->EX.ma.info.cannot_accept && this->ID.op[i].use_mem()) // メモリが使用中
        || (this->EX.mfp.info.cannot_accept && this->ID.op[i].use_multicycle_fpu()); // FPU(マルチサイクル)が使用中

    return raw_hazard || waw_hazard || structural_hazard;
}

// 書き込みポート数が不十分な場合のハザード検出 (insufficient write port)
bool Configuration::iwp_hazard_detector(unsigned int i){
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
        return this->ID.op[0].use_rd_fp() && ((ma_wb_fp && mfp_wb_fp) || (mfp_wb_fp && pfp_wb_fp) || (pfp_wb_fp && ma_wb_fp));
    }else if(i == 1){
        return
            (this->ID.op[0].use_rd_int() && this->ID.op[1].use_rd_int() && this->EX.ma.info.is_willing_but_not_ready_int)
            || (this->ID.op[1].use_rd_fp() && ((ma_wb_fp && mfp_wb_fp) || (mfp_wb_fp && pfp_wb_fp) || (pfp_wb_fp && ma_wb_fp)))
            || (this->ID.op[0].use_rd_fp() && this->ID.op[1].use_rd_fp() && (ma_wb_fp || mfp_wb_fp || pfp_wb_fp));
    }else{
        std::exit(EXIT_FAILURE);
    }
}

// WBステージに命令を渡す
void Configuration::wb_req(Instruction inst){
    if(!this->WB.inst[0].has_value()){
        this->WB.inst[0] = inst;
    }else if(!this->WB.inst[1].has_value()){
        this->WB.inst[1] = inst;
    }else{
        exit_with_output("too many requests for WB (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(inst.pc)))) : "") + ")");
    }
}

void Configuration::EX_stage::EX_al::exec(){
    if(this->inst.op.is_nop()) return;
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
        case o_nop:
            return;
        default:
            exit_with_output("invalid operation for AL (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

void Configuration::EX_stage::EX_br::exec(){
    if(this->inst.op.is_nop()) return;
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
        case o_nop:
            return;
        default:
            exit_with_output("invalid operation for BR (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

void Configuration::EX_stage::EX_ma::exec(){
    if(this->inst.op.is_nop()) return;
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
        default:
            exit_with_output("invalid operation for MA (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

bool Configuration::EX_stage::EX_ma::available(){
    return this->cycle_count == 1; // 仮の値で0
}

// void exec_inst(Instruction inst){
//     switch(inst.op.type){
        // case o_fadd:
        //     if(is_ieee){
        //         write_reg_fp(op.rd, read_reg_fp(op.rs1) + read_reg_fp(op.rs2));
        //     }else{
        //         write_reg_fp_32(op.rd, fadd(read_reg_fp_32(op.rs1), read_reg_fp_32(op.rs2)));
        //     }
        //     ++op_type_count[o_fadd];
        //     pc += 4;
        //     return;
        // case o_fsub:
        //     if(is_ieee){
        //         write_reg_fp(op.rd, read_reg_fp(op.rs1) - read_reg_fp(op.rs2));
        //     }else{
        //         write_reg_fp_32(op.rd, fsub(read_reg_fp_32(op.rs1), read_reg_fp_32(op.rs2)));
        //     }
        //     ++op_type_count[o_fsub];
        //     pc += 4;
        //     return;
        // case o_fmul:
        //     if(is_ieee){
        //         write_reg_fp(op.rd, read_reg_fp(op.rs1) * read_reg_fp(op.rs2));
        //     }else{
        //         write_reg_fp_32(op.rd, fmul(read_reg_fp_32(op.rs1), read_reg_fp_32(op.rs2)));
        //     }
        //     ++op_type_count[o_fmul];
        //     pc += 4;
        //     return;
        // case o_fdiv:
        //     if(is_ieee){
        //         write_reg_fp(op.rd, read_reg_fp(op.rs1) / read_reg_fp(op.rs2));
        //     }else{
        //         write_reg_fp_32(op.rd, fdiv(read_reg_fp_32(op.rs1), read_reg_fp_32(op.rs2)));
        //     }
        //     ++op_type_count[o_fdiv];
        //     pc += 4;
        //     return;
        // case o_fsqrt:
        //     if(is_ieee){
        //         write_reg_fp(op.rd, std::sqrt(read_reg_fp(op.rs1)));
        //     }else{
        //         write_reg_fp_32(op.rd, fsqrt(read_reg_fp_32(op.rs1)));
        //     }
        //     ++op_type_count[o_fsqrt];
        //     pc += 4;
        //     return;
        // case o_fcvtif:
        //     if(is_ieee){
        //         write_reg_fp(op.rd, static_cast<float>(read_reg_fp_32(op.rs1).i));
        //     }else{
        //         write_reg_fp_32(op.rd, itof(read_reg_fp_32(op.rs1)));
        //     }
        //     ++op_type_count[o_fcvtif];
        //     pc += 4;
        //     return;
        // case o_fcvtfi:
        //     if(is_ieee){
        //         write_reg_fp(op.rd, static_cast<int>(std::nearbyint(read_reg_fp(op.rs1))));
        //     }else{
        //         write_reg_fp_32(op.rd, ftoi(read_reg_fp_32(op.rs1)));
        //     }
        //     ++op_type_count[o_fcvtfi];
        //     pc += 4;
        //     return;
        // case o_fmvff:
        //     write_reg_fp_32(op.rd, read_reg_fp_32(op.rs1));
        //     ++op_type_count[o_fmvff];
        //     pc += 4;
        //     return;
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
        // case o_fmvif:
        //     write_reg_fp_32(op.rd, read_reg_32(op.rs1));
        //     ++op_type_count[o_fmvif];
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
