#include <sim.hpp>
#include <common.hpp>
#include <util.hpp>
#include <fpu.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <boost/lockfree/queue.hpp>
#include <boost/bimap/bimap.hpp>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <regex>
#include <iomanip>
#include <boost/program_options.hpp>
#include <chrono>

namespace po = boost::program_options;

/* グローバル変数 */
// 内部処理関係
std::vector<Operation> op_list; // 命令のリスト(PC順)
Bit32 reg_list[32]; // 整数レジスタのリスト
Bit32 reg_fp_list[32]; // 浮動小数レジスタのリスト
Bit32 *memory; // メモリ領域

unsigned int pc = 0; // プログラムカウンタ
unsigned long long op_count = 0; // 命令のカウント
constexpr unsigned long long max_op_count = 10000000000;
int mem_size = 100; // メモリサイズ
constexpr int max_mem_size = 98304; // FPGAに乗る最大メモリサイズ(393216B)
bool memory_exceeding_flag = false;

int port = 20214; // 通信に使うポート番号
std::queue<Bit32> receive_buffer; // 外部通信での受信バッファ
boost::lockfree::queue<Bit32> send_buffer(3*1e6); // 外部通信での受信バッファ

cache_line *cache; // キャッシュ本体
unsigned int index_width = 6; // インデックス幅 (キャッシュのライン数=2^n)
unsigned int offset_width = 6; // オフセット幅 (キャッシュのブロックサイズ=2^n)
unsigned int cache_read_times = 0; // キャッシュへのアクセス回数(読み込み)
unsigned int cache_hit_times = 0; // キャッシュのヒット率

// シミュレーションの制御
bool is_debug = false; // デバッグモード
bool is_detailed_debug = false; // 詳細なデバッグモード
bool is_info_output = false; // 出力モード
bool is_bin = false; // バイナリファイルモード
bool is_skip = false; // ブートローディングの過程をスキップするモード
bool is_bootloading = false; // ブートローダ対応モード
bool is_raytracing = false; // レイトレ専用モード
bool is_ieee = false; // IEEE754に従って浮動小数演算を行うモード
std::string filename; // 処理対象のファイル名
bool is_preloading = false; // バッファのデータを予め取得しておくモード
std::string preload_filename; // プリロード対象のファイル名

// 統計・出力関連
unsigned long long op_type_count[op_type_num]; // 各命令の実行数
int input_line_num = 0; // ファイルの行数
unsigned int *line_exec_count; // 行ごとの実行回数
int max_x2 = 0;
int memory_used = 0;
constexpr int stack_border = 4000;
unsigned int *mem_accessed_read; // メモリのreadによるアクセス回数
unsigned int *mem_accessed_write; // メモリのwriteによるアクセス回数
unsigned int stack_accessed_read_count = 0; // スタックのreadによるアクセスの総回数
unsigned int stack_accessed_write_count = 0; // スタックのwriteによるアクセスの総回数
unsigned int heap_accessed_read_count = 0; // ヒープのreadによるアクセスの総回数
unsigned int heap_accessed_write_count = 0; // ヒープのwriteによるアクセスの総回数
double exec_time; // 実行時間
double op_per_sec; // 秒あたりの実行命令数
std::string timestamp;

// 処理用のデータ構造
bimap_t bp_to_id; // ブレークポイントと命令idの対応
bimap_t label_to_id; // ラベルと命令idの対応
bimap_t2 id_to_line; // 命令idと行番号の対応
bimap_t bp_to_id_loaded; // ロードされたもの専用
bimap_t label_to_id_loaded;
bimap_t2 id_to_line_loaded;

// フラグ
bool simulation_end = false; // シミュレーション終了判定
bool breakpoint_skip = false; // ブレークポイント直後の停止回避
bool loop_flag = false;

// ブートローダ処理用の変数
bool is_waiting_for_lnum = false; // ライン数の受信を待っている途中のフラグ
bool is_loading_codes = false; // 命令ロード中のフラグ
int loading_id = 100; // 読み込んでいる命令のid

// ターミナルへの出力用
std::string head = "\x1b[1m[sim]\x1b[0m ";


int main(int argc, char *argv[]){
    // コマンドライン引数をパース
    po::options_description opt("./sim option");
	opt.add_options()
        ("help,h", "show help")
        ("file,f", po::value<std::string>(), "filename")
        ("debug,d", "debug mode")
        ("detailed", "detailed-debug mode")
        ("info,i", "information-output mode")
        ("bin,b", "binary-input mode")
        ("port,p", po::value<int>(), "port number")
        ("mem,m", po::value<int>(), "memory size")
        ("raytracing,r", "specialized for ray-tracing program")
        ("skip,s", "skipping bootloading")
        ("boot", "bootloading mode")
        ("preload", po::value<std::string>()->implicit_value("contest"), "data preload")
        ("ieee", "IEEE754 mode");
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
    if(vm.count("detailed")){
        is_debug = true;
        is_detailed_debug = true;
    }
    if(vm.count("info")) is_info_output = true;
    if(vm.count("bin")) is_bin = true;
    if(vm.count("port")) port = vm["port"].as<int>();
    if(vm.count("mem")) mem_size = vm["mem"].as<int>();
    if(vm.count("raytracing")) is_raytracing = true;
    if(vm.count("skip")) is_skip = true;
    if(vm.count("boot")) is_bootloading = true;
    if(vm.count("preload")){
        is_preloading = true;
        preload_filename = vm["preload"].as<std::string>();
    };
    if(vm.count("ieee")) is_ieee = true;

    // タイムスタンプの取得
    time_t t = time(nullptr);
    tm* time = localtime(&t);
    std::stringstream ss_timestamp;
    ss_timestamp << "20" << time -> tm_year - 100;
    ss_timestamp << "_" << std::setw(2) << std::setfill('0') <<  time -> tm_mon + 1;
    ss_timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_mday;
    ss_timestamp << "_" << std::setw(2) << std::setfill('0') <<  time -> tm_hour;
    ss_timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_min;
    ss_timestamp << "_" << std::setw(2) << std::setfill('0') <<  time -> tm_sec;
    timestamp = ss_timestamp.str();


    // ここからシミュレータの処理開始
    std::cout << head << "simulation start" << (is_detailed_debug ? " (in detailed-debug-mode)" : (is_debug ? " (in debug-mode)" : "")) << std::endl;
    auto start = std::chrono::system_clock::now();

    // ブートローダ処理をスキップする場合の処理
    if(is_skip){
        op_list.resize(100);
        pc = 100 * 4;
    }

    // レイトレを処理する場合は予めreserve
    if(is_raytracing){
        op_list.reserve(12000);
        mem_size = 2500000; // 10MB
    }

    // レジスタの初期化
    for(int i=0; i<32; ++i){ // レジスタをクリア
        reg_list[i] = Bit32(0);
        reg_fp_list[i] = Bit32(0);
    }

    // メモリ領域の確保
    memory = (Bit32*) calloc(mem_size, sizeof(Bit32));

    // RAMの初期化
    init_ram();

    // 統計データの初期化
    if(is_detailed_debug){
        mem_accessed_read = (unsigned int*) calloc(mem_size, sizeof(unsigned int));
        mem_accessed_write = (unsigned int*) calloc(mem_size, sizeof(unsigned int));
    }

    // キャッシュの初期化
    if(is_debug){
        cache = (cache_line*) calloc(1 << index_width, sizeof(cache_line));
    }
    
    // バッファのデータのプリロード
    if(is_preloading){
        preload_filename = "./data/" + preload_filename + ".bin";
        std::ifstream preload_file(preload_filename, std::ios::in | std::ios::binary);
        if(!preload_file){
            std::cerr << head_error << "could not open " << preload_filename << std::endl;
            std::exit(EXIT_FAILURE);
        }
        unsigned char c;
        while(!preload_file.eof()){
            preload_file.read((char*) &c, sizeof(char)); // 8bit取り出す
            receive_buffer.push(Bit32(static_cast<int>(c)));
        }
        std::cout << head << "preloaded data to the receive-buffer from " + preload_filename << std::endl;
    }

    // ファイルを読む
    std::string input_filename;
    if(is_bootloading){
        input_filename = "./code/bootloader";
    }else{
        input_filename = "./code/" + filename + (is_bin ? ".bin" : (is_debug ? ".dbg" : ""));
    }
    std::ifstream input_file(input_filename);
    if(!input_file.is_open()){
        std::cerr << head_error << "could not open " << input_filename << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // ファイルの各行をパースしてop_listに追加
    std::string code;
    std::string code_keep; // 空白でない最後の行を保存
    int code_id = is_skip ? 100 : 0;
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
        if(is_debug){
            if(std::regex_match(code_keep, match, std::regex("^\\d{32}@(-?\\d+).*$"))){
                input_line_num = std::stoi(match[1].str());
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

    if(is_detailed_debug){
        line_exec_count = (unsigned int*) calloc(input_line_num, sizeof(unsigned int));
    }

    auto end = std::chrono::system_clock::now();
    auto msec = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << head << "time elapsed (preparation): " << msec << std::endl;

    // コマンドの受け付けとデータ受信処理を別々のスレッドで起動
    std::thread t1(simulate);
    std::thread t2(receive_data);
    cancel_flag flg;
    std::thread t3(send_data, std::ref(flg));
    t1.join();
    t2.detach();
    flg.signal();
    t3.join();

    // 実行結果の情報を出力
    if(is_info_output || is_detailed_debug) output_info();
    
    // レイトレの場合は画像も出力
    if(is_raytracing){
        if(!send_buffer.empty()){
            std::string output_filename = "./out/output_" + timestamp + ".ppm";
            std::ofstream output_file(output_filename);
            if(!output_file){
                std::cerr << head_error << "could not open " << output_filename << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::stringstream output;
            Bit32 b32;
            while(send_buffer.pop(b32)){
                output << (unsigned char) b32.i;
            }
            output_file << output.str();
            std::cout << head << "output image written in " << output_filename << std::endl;
        }else{
            std::cout << head_error << "send-buffer is empty" << std::endl;
        }
    }
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
    }else{ // デバッグなしモード
        exec_command("run -t");
    }

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
        breakpoint_skip = false;
        if(simulation_end){
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }else{
            bool end_flag = false;
            if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
            std::cout << "pc " << pc << " (line " << id_to_line.left.at(id_of_pc(pc)) << ") " << op_list[id_of_pc(pc)].to_string() << std::endl;
            exec_op();
            if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
            ++op_count;

            if(end_flag){
                simulation_end = true;
                std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(s|(step))\\s*$"))){ // step
        breakpoint_skip = false;
        Operation op = op_list[id_of_pc(pc)];
        if(
            ((op.opcode == 9) && (op.funct == -1) && (op.rs2 == -1) && (op.rd == 1)) ||
            ((op.opcode == 10) && (op.funct == -1) && (op.rs1 = -1) && (op.rs2 == -1) && (op.rd == 1))
        ){
            unsigned int old_pc = pc;
            no_info = true;
            exec_command("break " + std::to_string(id_to_line.left.at(id_of_pc(pc + 4))) + " __ret");
            exec_command("continue __ret");
            exec_command("delete __ret");
            no_info = false;
            std::cout << head_info << "step execution around pc " << old_pc << " (line " << id_to_line.left.at(id_of_pc(old_pc)) << ") " << op_list[id_of_pc(old_pc)].to_string() << std::endl;
            std::cout << head_info << "returned to pc " << pc << " (line " << id_to_line.left.at(id_of_pc(pc)) << ") " << op_list[id_of_pc(pc)].to_string() << std::endl;
        }else{
            exec_command("do");
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(do))\\s+(\\d+)\\s*$"))){ // do N
        breakpoint_skip = false;
        if(simulation_end){
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }else{
            bool end_flag = false;
            for(int i=0; i<std::stoi(match[3].str()); ++i){
                if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                exec_op();
                if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                ++op_count;
                
                if(end_flag){
                    simulation_end = true;
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(r|(run))(\\s+(-t))?\\s*$"))){ // run
        bool is_time_measuring = match[4].str() == "-t";
        loop_flag = true;
        breakpoint_skip = false;
        if(simulation_end){
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }else{
            bool end_flag = false;
            auto start = std::chrono::system_clock::now();
            while(true){
                if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                exec_op();
                if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                ++op_count;
                
                if(end_flag){
                    simulation_end = true;
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
            if(is_time_measuring){
                auto end = std::chrono::system_clock::now();
                exec_time = std::chrono::duration<double>(end - start).count();
                std::cout << head << "time elapsed (execution): " << exec_time << std::endl;
                std::cout << head << "operation count: " << op_count << std::endl;
                op_per_sec = static_cast<double>(op_count) / exec_time;
                std::cout << head << "operations per second: " << op_per_sec << std::endl;
            }
            // メモリ使用量を保存しておく
            if(is_raytracing){
                memory_used = reg_list[3].i;
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(init))\\s*$"))){ // init
        breakpoint_skip = false;
        simulation_end = false;
        pc = is_skip ? 100 : 0; // PCを0にする
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
                exec_op();
                if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                ++op_count;
                
                if(end_flag){
                    simulation_end = true;
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(c|(continue))\\s+(([a-zA-Z_]\\w*(.\\d+)*))\\s*$"))){ // continue break
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
                            if(!no_info){
                                std::cout << head_info << "halt before breakpoint '" + bp << "' (line " << id_of_pc(pc) + 1 << ")" << std::endl;
                            }
                            loop_flag = false;
                            breakpoint_skip = true; // ブレークポイント直後に再度continueした場合はスキップ
                            break;
                        }
                    }

                    if(is_end(op_list[id_of_pc(pc)])) end_flag = true; // self-loopの場合は、1回だけ実行して終了とする
                    exec_op();
                    if(id_of_pc(pc) >= op_list.size()) end_flag = true; // 最後の命令に到達した場合も終了とする
                    ++op_count;
                    
                    if(end_flag){
                        simulation_end = true;
                        std::cout << head_info << "did not encounter breakpoint '" << bp << "'" << std::endl;
                        std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                        break;
                    }
                }
            }else{
                std::cout << head_error << "breakpoint '" << bp << "' has not been set" << std::endl;
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(info))\\s*$"))){ // info
        std::cout << "operations executed: " << op_count << std::endl;
        if(simulation_end){
            std::cout << "next: (no operation left to be simulated)" << std::endl;
        }else{
            std::cout << "next: pc " << pc << " (line " << id_to_line.left.at(id_of_pc(pc)) << ") " << op_list[id_of_pc(pc)].to_string() << std::endl;
        }
        if(bp_to_id.empty()){
            std::cout << "breakpoints: (no breakpoint found)" << std::endl;
        }else{
            std::cout << "breakpoints:" << std::endl;
            for(auto x : bp_to_id.left) {
                std::cout << "  " << x.first << " (pc " << x.second * 4 << ", line " << id_to_line.left.at(x.second) << ")" << std::endl;
            }
        }
        std::cout << "execution stat:" << std::endl;
        for(int i=0; i<op_type_num; ++i){
            std::cout << "  " << string_of_otype(static_cast<Otype>(i)) << ": " << op_type_count[i] << std::endl;
        }
    // }else if(std::regex_match(cmd, std::regex("^\\s*(p|(print))\\s*$"))){ // print
    //
    }else if(std::regex_match(cmd, std::regex("^\\s*(p|(print))\\s+reg\\s*$"))){ // print reg
        print_reg();
        print_reg_fp();
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))\\s+buf(\\s+(\\d+))?\\s*$"))){ // print buf
        if(receive_buffer.empty()){
            std::cout << "receive buffer: (empty)" << std::endl;
        }else{
            int size;
            if(match[4].str() == ""){
                size = 10;
            }else{
                size = std::stoi(match[4].str());
            }
            std::cout << "receive buffer:\n  ";
            print_queue(receive_buffer, size);
        }
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
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(out)(\\s+(-p|-b))?(\\s+(-f)\\s+(\\w+))?\\s*$"))){ // out (option)
        /* notice: これは臨時のコマンド */
        if(!send_buffer.empty()){
            bool is_ppm = match[3].str() == "-p";
            bool is_bin = match[3].str() == "-b";
            
            // ファイル名関連の処理
            std::string ext = is_ppm ? ".ppm" : (is_bin ? ".bin" : ".txt");
            std::string filename;
            if(match[4].str() == ""){
                filename = "output";
            }else{
                filename = match[6].str();
            }

            std::string output_filename = "./out/" + filename + "_" + timestamp + ext;
            std::ofstream output_file;
            if(is_bin){
                output_file.open(output_filename, std::ios::out | std::ios::binary | std::ios::trunc);
            }else{
                output_file.open(output_filename);
            }
            if(!output_file){
                std::cerr << head_error << "could not open " << output_filename << std::endl;
                std::exit(EXIT_FAILURE);
            }

            std::stringstream output;
            Bit32 b32;
            if(is_ppm){
                while(send_buffer.pop(b32)){ // todo: send_bufferを破壊しないようにしたい
                    output << (unsigned char) b32.i;
                }
            }else if(is_bin){
                while(send_buffer.pop(b32)){
                    output.write((char*) &b32, sizeof(char)); // 8bitだけ書き込む
                }
            }else{
                while(send_buffer.pop(b32)){
                    output << b32.to_string(Stype::t_hex) << std::endl;
                }
            }
            output_file << output.str();
            std::cout << head_info << "send-buffer data written in " << output_filename << std::endl;
        }else{
            std::cout << head_error << "send-buffer is empty" << std::endl;
        }
    }else{
        std::cout << head_error << "invalid command" << std::endl;
    }

    return res;
}

// 命令を実行し、PCを変化させる
void exec_op(){
    Operation op(op_list[id_of_pc(pc)]);

    // 詳細デバッグモードの場合、行数ごとの実行回数を更新
    if(is_detailed_debug){
        int l = id_to_line.left.at(id_of_pc(pc));
        if(l > 0) ++line_exec_count[l-1];
    }

    // ブートローダ用処理(bootloader.sの内容に依存しているので注意！)
    if(is_bootloading){
        if(op.opcode == 4 && op.funct == 2 && op.rs1 == 0 && op.rs2 == 5 && op.rd == -1 && op.imm == 0){ // std %x5
            int x5 = reg_list[5].i;
            if(x5 == 153){ // 0x99
                is_waiting_for_lnum = true;
            }else if(x5 == 170){ // 0xaa
                is_loading_codes = false;
                is_bootloading = false; // ブートローダ用処理の終了
                if(is_debug){
                    id_to_line = std::move(id_to_line_loaded);
                    label_to_id = std::move(label_to_id_loaded);
                    bp_to_id = std::move(bp_to_id_loaded);
                    std::queue<Bit32>().swap(receive_buffer);
                    std::cout << head_info << "bootloading end" << std::endl;
                }
            }
        }

        if(is_waiting_for_lnum && op.opcode == 6 && op.funct == 0 && op.rs1 == 6 && op.rs2 == -1 && op.rd == 7 && op.imm == 0){ // addi %x7, %x6, 0
            is_waiting_for_lnum = false;
            int loaded_op_num = reg_list[6].i / 4;
            if(is_debug){
                std::cout << head_info << "operations to be loaded: " << loaded_op_num << std::endl;
                is_loading_codes = true; // 命令のロード開始
            }
            op_list.resize(100 + loaded_op_num); // 受け取る命令の数に合わせてop_listを拡大
        }
    }

    if(is_raytracing && op_count >= max_op_count){
        exit_with_output("too many operations executed for raytracing program");
    }

    switch(op.opcode){
        case 0: // op
            switch(op.funct){
                case 0: // add
                    write_reg(op.rd, read_reg(op.rs1) + read_reg(op.rs2));
                    ++op_type_count[Otype::o_add];
                    pc += 4;
                    return;
                case 1: // sub
                    write_reg(op.rd, read_reg(op.rs1) - read_reg(op.rs2));
                    ++op_type_count[Otype::o_sub];
                    pc += 4;
                    return;
                case 2: // sll
                    write_reg(op.rd, read_reg(op.rs1) << read_reg(op.rs2));
                    ++op_type_count[Otype::o_sll];
                    pc += 4;
                    return;
                case 3: // srl
                    write_reg(op.rd, static_cast<unsigned int>(read_reg(op.rs1)) >> read_reg(op.rs2));
                    ++op_type_count[Otype::o_srl];
                    pc += 4;
                    return;
                case 4: // sra
                    write_reg(op.rd, read_reg(op.rs1) >> read_reg(op.rs2)); // todo: 処理系依存
                    ++op_type_count[Otype::o_sra];
                    pc += 4;
                    return;
                case 5: // and
                    write_reg(op.rd, read_reg(op.rs1) & read_reg(op.rs2));
                    ++op_type_count[Otype::o_and];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 1: // op_fp todo: 仕様に沿っていないので注意
            switch(op.funct){
                case 0: // fadd
                    if(is_ieee){
                        write_reg_fp(op.rd, read_reg_fp(op.rs1) + read_reg_fp(op.rs2));
                    }else{
                        write_reg_fp_32(op.rd, fadd(read_reg_fp_32(op.rs1), read_reg_fp_32(op.rs2)));
                    }
                    ++op_type_count[Otype::o_fadd];
                    pc += 4;
                    return;
                case 1: // fsub
                    if(is_ieee){
                        write_reg_fp(op.rd, read_reg_fp(op.rs1) - read_reg_fp(op.rs2));
                    }else{
                        write_reg_fp_32(op.rd, fsub(read_reg_fp_32(op.rs1), read_reg_fp_32(op.rs2)));
                    }
                    ++op_type_count[Otype::o_fsub];
                    pc += 4;
                    return;
                case 2: // fmul
                    if(is_ieee){
                        write_reg_fp(op.rd, read_reg_fp(op.rs1) * read_reg_fp(op.rs2));
                    }else{
                        write_reg_fp_32(op.rd, fmul(read_reg_fp_32(op.rs1), read_reg_fp_32(op.rs2)));
                    }
                    ++op_type_count[Otype::o_fmul];
                    pc += 4;
                    return;
                case 3: // fdiv
                    if(is_ieee){
                        write_reg_fp(op.rd, read_reg_fp(op.rs1) / read_reg_fp(op.rs2));
                    }else{
                        write_reg_fp_32(op.rd, fdiv(read_reg_fp_32(op.rs1), read_reg_fp_32(op.rs2)));
                    }
                    ++op_type_count[Otype::o_fdiv];
                    pc += 4;
                    return;
                case 4: // fsqrt
                    if(is_ieee){
                        write_reg_fp(op.rd, std::sqrt(read_reg_fp(op.rs1)));
                    }else{
                        write_reg_fp_32(op.rd, fsqrt(read_reg_fp_32(op.rs1)));
                    }
                    ++op_type_count[Otype::o_fsqrt];
                    pc += 4;
                    return;
                case 5: // fcvt.i.f
                    if(is_ieee){
                        write_reg_fp(op.rd, static_cast<float>(read_reg_fp_32(op.rs1).i));
                    }else{
                        write_reg_fp_32(op.rd, itof(read_reg_fp_32(op.rs1)));
                    }
                    ++op_type_count[Otype::o_fcvtif];
                    pc += 4;
                    return;
                case 6: // fcvt.f.i
                    if(is_ieee){
                        write_reg_fp(op.rd, static_cast<int>(std::nearbyint(read_reg_fp(op.rs1))));
                    }else{
                        write_reg_fp_32(op.rd, ftoi(read_reg_fp_32(op.rs1)));
                    }
                    ++op_type_count[Otype::o_fcvtfi];
                    pc += 4;
                    return;
            }
            break;
        case 2: // branch
            switch(op.funct){
                case 0: // beq
                    read_reg(op.rs1) == read_reg(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    ++op_type_count[Otype::o_beq];
                    return;
                case 1: // blt
                    read_reg(op.rs1) < read_reg(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    ++op_type_count[Otype::o_blt];
                    return;
                default: break;
            }
            break;
        case 3: // branch_fp
            switch(op.funct){
                case 2: // fbeq
                    read_reg_fp(op.rs1) == read_reg_fp(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    ++op_type_count[Otype::o_fbeq];
                    return;
                case 3: // fblt
                    read_reg_fp(op.rs1) < read_reg_fp(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    ++op_type_count[Otype::o_fblt];
                    return;
                default: break;
            }
            break;
        case 4: // store
            switch(op.funct){
                case 0: // sw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        write_memory((read_reg(op.rs1) + op.imm) / 4, Bit32(read_reg(op.rs2)));
                    }else{
                        exit_with_output("address of store operation should be multiple of 4 [sw] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
                    }
                    ++op_type_count[Otype::o_sw];
                    pc += 4;
                    return;
                case 1: // si
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        op_list[(read_reg(op.rs1) + op.imm) / 4] = Operation(read_reg(op.rs2));
                    }else{
                        exit_with_output("address of store operation should be multiple of 4 [si] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
                    }
                    ++op_type_count[Otype::o_si];
                    pc += 4;
                    return;
                case 2: // std
                    send_buffer.push(read_reg(op.rs2));
                    ++op_type_count[Otype::o_std];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 5: // store_fp
            switch(op.funct){
                case 0: // fsw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        write_memory((read_reg(op.rs1) + op.imm) / 4, Bit32(read_reg_fp(op.rs2)));
                    }else{
                        exit_with_output("address of store operation should be multiple of 4 [fsw] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
                    }
                    ++op_type_count[Otype::o_fsw];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 6: // op_imm
            switch(op.funct){
                case 0: // addi
                    write_reg(op.rd, read_reg(op.rs1) + op.imm);
                    ++op_type_count[Otype::o_addi];
                    pc += 4;
                    return;
                case 2: // slli
                    write_reg(op.rd, read_reg(op.rs1) << op.imm);
                    ++op_type_count[Otype::o_slli];
                    pc += 4;
                    return;
                case 3: // srli
                    write_reg(op.rd, static_cast<unsigned int>(read_reg(op.rs1)) >> op.imm);
                    ++op_type_count[Otype::o_srli];
                    pc += 4;
                    return;
                case 4: // srai
                    write_reg(op.rd, read_reg(op.rs1) >> op.imm); // todo: 処理系依存
                    ++op_type_count[Otype::o_srai];
                    pc += 4;
                    return;
                case 5: // andi
                    write_reg(op.rd, read_reg(op.rs1) & op.imm);
                    ++op_type_count[Otype::o_andi];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 7: // load
            switch(op.funct){
                case 0: // lw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        write_reg(op.rd, read_memory((read_reg(op.rs1) + op.imm) / 4).i);
                    }else{
                        exit_with_output("address of load operation should be multiple of 4 [lw] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
                    }
                    ++op_type_count[Otype::o_lw];
                    pc += 4;
                    return;
                case 1: // lre
                    write_reg(op.rd, receive_buffer.empty() ? 1 : 0);
                    pc += 4;
                    ++op_type_count[Otype::o_lre];
                    return;
                case 2: // lrd
                    if(!receive_buffer.empty()){
                        write_reg(op.rd, receive_buffer.front().i);
                        receive_buffer.pop();
                    }else{
                        exit_with_output("receive buffer is empty [lrd] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
                    }
                    ++op_type_count[Otype::o_lrd];
                    pc += 4;
                    return;
                case 3: // ltf
                    write_reg(op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
                    ++op_type_count[Otype::o_ltf];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 8: // load_fp
            switch(op.funct){
                case 0: // flw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        write_reg_fp(op.rd, read_memory((read_reg(op.rs1) + op.imm) / 4).f);
                    }else{
                        exit_with_output("address of load operation should be multiple of 4 [flw] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
                    }
                    ++op_type_count[Otype::o_flw];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 9: // jalr
            {
                unsigned next_pc = pc + 4;
                pc = read_reg(op.rs1) + op.imm * 4;
                write_reg(op.rd, next_pc);
                ++op_type_count[Otype::o_jalr];
            }
            return;
        case 10: // jal
            write_reg(op.rd, pc + 4);
            ++op_type_count[Otype::o_jal];
            pc += op.imm * 4;
            return;
        case 11: // lui
            switch(op.funct){
                case 0: // lui
                    write_reg(op.rd, op.imm << 12);
                    ++op_type_count[Otype::o_lui];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 12: // itof
            switch(op.funct){
                case 0: // fmv.i.f
                    write_reg_fp(op.rd, Bit32(read_reg(op.rs1)).f);
                    ++op_type_count[Otype::o_fmvif];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 13: // ftoi
            switch(op.funct){
                case 0: // fmv.f.i
                    write_reg(op.rd, Bit32(read_reg_fp(op.rs1)).i);
                    ++op_type_count[Otype::o_fmvfi];
                    pc += 4;
                    return;
                default: break;
            }
            break;
        default: break;
    }

    exit_with_output("error in executing the code (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(pc)))) : "") + ")");
}

// データの受信
void receive_data(){
    // 受信設定
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    // 受信の準備
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr));
    listen(server_socket, 5);

    // クライアントの情報を保持する変数など
    struct sockaddr_in client_addr;
    int client_socket;
    int client_addr_size = sizeof(client_addr);
    char buf[32];
    // int recv_len;

    while(true){
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_size);
        recv(client_socket, buf, 32, 0);
        std::string data(buf);
        memset(buf, '\0', sizeof(buf)); // バッファをクリア
        for(int i=0; i<4; ++i){ // big endianで受信したものとしてバッファに追加
            receive_buffer.push(bit32_of_data(data.substr(i * 8, 8)));
        }

        // while((recv_len = recv(client_socket, buf, 64, 0)) != 0){
        //     send(client_socket, "0", 1, 0); // 成功したらループバック

        //     // 受信したデータの処理
        //     std::string data(buf);
        //     memset(buf, '\0', sizeof(buf)); // バッファをクリア
        //     // if(is_debug){
        //     //     std::cout << head_data << "received " << data << std::endl;
        //     //     if(!loop_flag){
        //     //         std::cout << "\033[2D# " << std::flush;
        //     //     }
        //     // }

        //     if(is_bootloading && data[0] == 'n'){
        //         filename = "boot-" + data.substr(1);
        //         if(is_debug){
        //             std::cout << head_info << "loading ./code/" << data.substr(1) << std::endl;
        //             std::cout << "\033[2D# " << std::flush;
        //         }
        //     }else if(is_loading_codes && data[0] == 't'){ // 渡されてきたラベル・ブレークポイントを処理
        //         // 命令ロード中の場合
        //         if(is_debug){
        //             std::string text = data.substr(1);
        //             std::smatch match;
        //             if(std::regex_match(text, match, std::regex("^@(-?\\d+)$"))){
        //                 id_to_line_loaded.insert(bimap_value_t2(loading_id, std::stoi(match[1].str())));
        //             }else if(std::regex_match(text, match, std::regex("^@(-?\\d+)#(([a-zA-Z_]\\w*(.\\d+)*))$"))){ // ラベルのみ
        //                 id_to_line_loaded.insert(bimap_value_t2(loading_id, std::stoi(match[1].str())));
        //                 label_to_id_loaded.insert(bimap_value_t(match[2].str(), loading_id));             
        //             }else if(std::regex_match(text, match, std::regex("^@(-?\\d+)!(([a-zA-Z_]\\w*(.\\d+)*))$"))){ // ブレークポイントのみ
        //                 id_to_line_loaded.insert(bimap_value_t2(loading_id, std::stoi(match[1].str())));
        //                 bp_to_id_loaded.insert(bimap_value_t(match[2].str(), loading_id));
        //             }else if(std::regex_match(text, match, std::regex("^@(-?\\d+)#(([a-zA-Z_]\\w*(.\\d+)*))!(([a-zA-Z_]\\w*(.\\d+)*))$"))){ // ラベルとブレークポイントの両方
        //                 id_to_line_loaded.insert(bimap_value_t2(loading_id, std::stoi(match[1].str())));
        //                 label_to_id_loaded.insert(bimap_value_t(match[2].str(), loading_id));
        //                 bp_to_id_loaded.insert(bimap_value_t(match[3].str(), loading_id));
        //             }else{
        //                 std::cerr << head_error << "could not parse the received code" << std::endl;
        //                 std::exit(EXIT_FAILURE);
        //             }

        //             ++loading_id;
        //         }else{
        //             std::cerr << head_error << "invalid data received (maybe: put -d option to ./server)" << std::endl;
        //             std::exit(EXIT_FAILURE);
        //         }
        //     }else{
        //         receive_buffer.push(bit32_of_data(data));
        //     }
        // }
        // close(client_socket);
    }
    return;
}

// データの送信
void send_data(cancel_flag& flg){
    if(!is_raytracing){
        // データ送信の準備
        struct in_addr host_addr;
        inet_aton("127.0.0.1", &host_addr);
        struct sockaddr_in opponent_addr; // 通信相手(./server)の情報
        opponent_addr.sin_family = AF_INET;
        opponent_addr.sin_port = htons(port+1);
        opponent_addr.sin_addr = host_addr;

        int client_socket = 0; // 送信用のソケット
        bool is_connected = false; // 通信が維持されているかどうかのフラグ
        Bit32 b32;
        std::string data;
        int res;
        char recv_buf[1];
        int res_len;

        while(!flg){
            while(send_buffer.pop(b32)){
                if(!is_connected){ // 接続されていない場合
                    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    res = connect(client_socket, (struct sockaddr *) &opponent_addr, sizeof(opponent_addr));
                    if(res == 0){
                        is_connected = true;
                    }else{
                        std::cout << head_error << "connection failed (check whether ./server has been started)" << std::endl;
                    }
                }
                
                data = binary_of_int(b32.i);
                send(client_socket, data.substr(24, 8).c_str(), 8, 0); // 下8bitだけ送信
                res_len = recv(client_socket, recv_buf, 1, 0);
                if(res_len == 0 || recv_buf[0] != '0'){ // 通信が切断された場合
                    std::cout << head_error << "data transmission failed (restart both ./sim and ./server)" << std::endl;
                    is_connected = false;
                }
            }
        }
        close(client_socket);
    }
    return;
}

// 情報の出力
void output_info(){
    // 実行情報
    std::string output_filename = "./info/" + filename + (is_debug ? "-dbg" : "") + "_" + timestamp + ".md";
    std::ofstream output_file(output_filename);
    if(!output_file){
        std::cerr << head_error << "could not open " << output_filename << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::stringstream ss;
    ss << "# basic stat" << std::endl;
    ss << "- operation count: " << op_count << std::endl;
    ss << "- execution time(s): " << exec_time << std::endl;
    ss << "- operations per second: " << op_per_sec << std::endl;
    if(is_debug){
        ss << "- cache:" << std::endl;
        ss << "\t- accessed: " << cache_read_times << std::endl;
        ss << "\t- hit rate: " << static_cast<double>(cache_hit_times) / cache_read_times << std::endl;
    }
    if(is_raytracing){
        ss << "- stack:" << std::endl;
        ss << "\t- size: " << max_x2 << std::endl;
        ss << "\t- read: " << stack_accessed_read_count << std::endl;
        ss << "\t- write: " << stack_accessed_write_count << std::endl;
        ss << "- heap: " << std::endl;
        ss << "\t- size: " << memory_used - stack_border << std::endl;
        ss << "\t- read: " << heap_accessed_read_count << std::endl;
        ss << "\t- write: " << heap_accessed_write_count << std::endl;
    }
    ss << std::endl;
    ss << "# operation stat" << std::endl;
    for(int i=0; i<op_type_num; ++i){
        ss << "- " << string_of_otype(static_cast<Otype>(i)) << ": " << op_type_count[i] << std::endl;
    }
    output_file << ss.str();
    std::cout << head << "simulation stat: " << output_filename << std::endl;

    if(is_detailed_debug){
        // メモリの情報
        std::string output_filename_mem = "./info/" + filename + "_mem_" + timestamp + ".csv";
        std::ofstream output_file_mem(output_filename_mem);
        if(!output_file_mem){
            std::cerr << head_error << "could not open " << output_filename_mem << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::stringstream ss_mem;
        int m = is_raytracing ? memory_used/4 : mem_size/4;
        ss_mem << "address,value,read,write" << std::endl;
        for(int i=0; i<m; ++i){
            ss_mem << i*4 << "," << memory[i].to_string(Stype::t_hex) << "," << mem_accessed_read[i] << "," << mem_accessed_write[i] << std::endl;
        }
        output_file_mem << ss_mem.str();
        std::cout << head << "memory info: " << output_filename_mem << std::endl;

        std::string output_filename_exec = "./info/" + filename + "_exec_" + timestamp + ".csv";
        std::ofstream output_file_exec(output_filename_exec);
        if(!output_file_exec){
            std::cerr << head_error << "could not open " << output_filename_exec << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::stringstream ss_exec;
        ss_exec << "line,exec" << std::endl;
        for(int i=0; i<input_line_num; ++i){
            ss_exec << i+1 << "," << line_exec_count[i] << std::endl;
        }
        output_file_exec << ss_exec.str();
        std::cout << head << "execution info: " << output_filename_exec << std::endl;
    }

    return;
}

// PCから命令IDへの変換(4の倍数になっていない場合エラー)
inline unsigned int id_of_pc(unsigned int n){
    if(n % 4 == 0){
        return n / 4;
    }else{
        exit_with_output("error with program counter: pc = " + std::to_string(n));
        return 0; // 実行されない
    }
}

// 整数レジスタから読む
inline int read_reg(int i){
    return i == 0 ? 0 : reg_list[i].i;
}

// 整数レジスタに書き込む
inline void write_reg(int i, int v){
    if (i != 0) reg_list[i] = Bit32(v);
    if(is_raytracing && i == 2 && v > max_x2) max_x2 = v;
    return;
}

// 浮動小数点数レジスタから読む
inline float read_reg_fp(int i){
    return i == 0 ? 0 : reg_fp_list[i].f;
}
// 浮動小数点数レジスタから読む(Bit32で)
inline Bit32 read_reg_fp_32(int i){
    return i == 0 ? Bit32(0) : reg_fp_list[i];
}

// 浮動小数点数レジスタに書き込む
inline void write_reg_fp(int i, float v){
    if (i != 0) reg_fp_list[i] = Bit32(v);
    return;
}
inline void write_reg_fp(int i, int v){
    if (i != 0) reg_fp_list[i] = Bit32(v);
    return;
}
// 浮動小数点数レジスタに書き込む(Bit32のまま)
inline void write_reg_fp_32(int i, Bit32 v){
    if (i != 0) reg_fp_list[i] = v;
    return;
}

inline Bit32 read_memory(int w){
    if(!memory_exceeding_flag && w >= max_mem_size){
        memory_exceeding_flag = true;
        std::cout << head_warning << "exceeded memory limit (384KiB)" << std::endl;
    }
    if(is_detailed_debug){
        ++mem_accessed_read[w];
        w < stack_border ? ++stack_accessed_read_count : ++heap_accessed_read_count;

        // キャッシュ関連の処理
        ++cache_read_times;
        unsigned int tag = ((w * 4) >> (index_width + offset_width)) & ((1 << (32 - (index_width + offset_width))) - 1);
        unsigned int index = ((w * 4) >> offset_width) & ((1 << index_width) - 1);
        // int offset = (w * 4) & ((1 << offset_width) - 1);
        cache_line line = cache[index];
        if(line.tag == tag){
            if(line.is_valid){
                ++cache_hit_times;
            }else{
                line.is_valid = true;
                cache[index] = line;
            }
        }else{
            line.is_valid = true;
            line.tag = tag;
            cache[index] = line;
        }
    }
    return memory[w];
}

inline void write_memory(int w, Bit32 v){
    if(!memory_exceeding_flag && w >= max_mem_size){
        memory_exceeding_flag = true;
        std::cout << head_warning << "exceeded memory limit (384KiB)" << std::endl;
    }
    if(is_detailed_debug){
        ++mem_accessed_write[w];
        w < stack_border ? ++stack_accessed_write_count : ++heap_accessed_write_count;
    }
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

// キューの表示
void print_queue(std::queue<Bit32> q, int n){
    while(!q.empty() && n > 0){
        std::cout << q.front().to_string() << "; ";
        q.pop();
        n--;
    }
    std::cout << std::endl;
}

// 終了時の無限ループ命令(jal x0, 0)であるかどうかを判定
inline bool is_end(Operation op){
    return (op.opcode == 10) && (op.funct == -1) && (op.rs1 = -1) && (op.rs2 == -1) && (op.rd == 0) && (op.imm == 0);
}

// 実効情報を表示したうえで異常終了
void exit_with_output(std::string msg){
    std::cout << head_error << msg << std::endl;
    if(is_info_output){
        std::cout << head << "outputting execution info until now" << std::endl;
        output_info();
    }else if(is_debug){
        std::cout << head << "execution info until now:" << std::endl;
        exec_command("info");
    }
    std::cout << head << "abnormal end" << std::endl;
    std::quick_exit(EXIT_FAILURE);
}
