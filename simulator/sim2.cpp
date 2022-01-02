#include <sim2.hpp>
#include <common.hpp>
#include <util.hpp>
#include <fpu.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/bimap/bimap.hpp>
#include <regex>
#include <iomanip>
#include <boost/program_options.hpp>
#include <chrono>

using ::Otype;
namespace po = boost::program_options;

/* グローバル変数 */
// 内部処理関係
std::vector<Operation> op_list; // 命令のリスト(PC順)
Reg reg_int; // 整数レジスタ
Reg reg_fp; // 浮動小数点数レジスタ
Bit32 *memory; // メモリ領域

unsigned long long* op_type_count;
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
// unsigned long long op_type_count[op_type_num]; // 各命令の実行数

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

    // 命令数カウントの初期化
    op_type_count = (unsigned long long*) calloc(op_type_num, sizeof(unsigned long long));

    // ここからシミュレータの処理開始
    std::cout << head << "simulation start" << std::endl;

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
        config.advance_clock(true);
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(do))\\s+(\\d+)\\s*$"))){ // do N
        for(int i=0; i<std::stoi(match[3].str()); ++i){
            config.advance_clock(false);
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(r|(run))(\\s+(-t))?\\s*$"))){ // run
        
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(init))\\s*$"))){ // init
        config = Configuration();
        op_count = 0; // 総実行命令数を0にする
        for(int i=0; i<32; ++i){ // レジスタをクリア
            reg_int = Reg();
            reg_fp = Reg();
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
        // print_reg();
        // print_reg_fp();
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
        unsigned int reg_no;
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
                std::cout << "\x1b[1m%x" << reg_no << "\x1b[0m: " << reg_int.read_32(reg_no).to_string(st) << std::endl;
            }else{ // float
                if(st == Stype::t_default) st = Stype::t_float; // デフォルトはfloat
                std::cout << "\x1b[1m%f" << reg_no << "\x1b[0m: " << reg_fp.read_32(reg_no).to_string(st) << std::endl;
            }
            cmd = match.suffix();
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))(\\s+(-w))?\\s+(m|mem)\\[(\\d+):(\\d+)\\]\\s*$"))){ // print mem[N:M]
        int start = std::stoi(match[6].str());
        int width = std::stoi(match[7].str());
        if(match[4].str() == "-w"){
            for(int i=start; i<start+width; ++i){
                std::cout << "mem[" << i << "]: " << memory[i].to_string() << std::endl;
            }
        }else{
            if(start % 4 == 0 && width % 4 == 0){
                for(int i=start/4; i<start/4+width/4; ++i){
                    std::cout << "mem[" << i << "]: " << memory[i].to_string() << std::endl;
                }
            }else{
                std::cout << head_error << "memory address should be multiple of 4 (hint: use `print -w m[N:M]` for word addressing)" << std::endl;   
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(s|(set))\\s+(x(\\d+))\\s+(\\d+)\\s*$"))){ // set reg N
        unsigned int reg_no = std::stoi(match[4].str());
        int val = std::stoi(match[5].str());
        if(0 < reg_no && reg_no < 31){
            reg_int.write_int(reg_no, val);
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

// PCから命令IDへの変換(4の倍数になっていない場合エラー)
unsigned int id_of_pc(int n){
    if(n % 4 == 0){
        return n / 4;
    }else{
        exit_with_output("error with program counter: pc = " + std::to_string(n));
        return 0; // 実行されない
    }
}

Bit32 read_memory(int w){
    return memory[w];
}

void write_memory(int w, Bit32 v){
    memory[w] = v;
}

// startからwidthぶん、4byte単位でメモリの内容を出力
void print_memory(int start, int width){
    for(int i=start; i<start+width; ++i){
        std::cout << "mem[" << i << "]: " << memory[i].to_string() << std::endl;
    }
    return;
}

// 終了時の無限ループ命令(jal x0, 0)であるかどうかを判定
bool is_end(Operation op){
    return (op.type == o_jal) && (op.rd == 0) && (op.imm == 0);
}

// 実効情報を表示したうえで異常終了
void exit_with_output(std::string msg){
    std::cout << head_error << msg << std::endl;
    std::cout << head << "abnormal end" << std::endl;
    std::quick_exit(EXIT_FAILURE);
}
