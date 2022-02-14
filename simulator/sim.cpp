#include <sim.hpp>
#include <common.hpp>
#include <unit.hpp>
#include <fpu.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/bimap/bimap.hpp>
#include <regex>
#include <boost/program_options.hpp>
#include <chrono>
#include <exception>
#ifdef DETAILED
#include <transmission.hpp>
#include <thread>
#endif

namespace po = boost::program_options;
using enum Otype;
using enum Stype;

/* グローバル変数 */
// 内部処理関係
std::vector<Operation> op_list; // 命令のリスト(PC順)
Reg reg_int; // 整数レジスタ
Reg reg_fp; // 浮動小数点数レジスタ
Memory memory; // メモリ
Fpu fpu; // FPU
Cache cache; // キャッシュ
Gshare branch_predictor(gshare_width); // 分岐予測器

unsigned int pc = 0; // プログラムカウンタ
unsigned int code_size = 0; // コードサイズ
constexpr unsigned long long max_op_count = 1000000000000;
int mem_size = 100; // メモリサイズ

unsigned int index_width = 6; // インデックス幅 (キャッシュのライン数=2^n)
unsigned int offset_width = 6; // オフセット幅 (キャッシュのブロックサイズ=2^n)

int port = 20214; // 通信に使うポート番号
TransmissionQueue receive_buffer; // 外部通信での受信バッファ
TransmissionQueue send_buffer; // 外部通信での送信バッファ


// シミュレーションの制御
int sim_state = sim_state_continue; // シミュレータの状態管理
bool is_debug = false; // デバッグモード
bool is_info_output = false; // 出力モード
bool is_bin = false; // バイナリファイルモード
bool is_stat = false; // 統計モード
bool is_cache_enabled = false; // キャッシュを考慮するモード
bool is_gshare_enabled = false; // 分岐予測を組み込むモード
bool is_skip = false; // ブートローディングの過程をスキップするモード
bool is_bootloading = false; // ブートローダ対応モード
bool is_raytracing = false; // レイトレ専用モード
bool is_ieee = false; // IEEE754に従って浮動小数演算を行うモード
bool is_cautious = false; // 例外処理などを慎重に行うモード
std::string filename; // 処理対象のファイル名
bool is_preloading = false; // バッファのデータを予め取得しておくモード
std::string preload_filename; // プリロード対象のファイル名

// 統計・出力関連
unsigned long long op_type_count[op_type_num]; // 各命令の実行数
int input_line_num = 0; // ファイルの行数
unsigned int *line_exec_count; // 行ごとの実行回数
int max_x2 = 0;
int memory_used = 0;
constexpr int stack_border = 1000;
unsigned long long *mem_accessed_read; // メモリのreadによるアクセス回数
unsigned long long *mem_accessed_write; // メモリのwriteによるアクセス回数
unsigned long long stack_accessed_read_count = 0; // スタックのreadによるアクセスの総回数
unsigned long long stack_accessed_write_count = 0; // スタックのwriteによるアクセスの総回数
unsigned long long heap_accessed_read_count = 0; // ヒープのreadによるアクセスの総回数
unsigned long long heap_accessed_write_count = 0; // ヒープのwriteによるアクセスの総回数
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
#ifdef DETAILED
std::string head = "\x1b[1m[sim(d)]\x1b[0m ";
#else
std::string head = "\x1b[1m[sim]\x1b[0m ";
#endif

int main(int argc, char *argv[]){
    // コマンドライン引数をパース
    po::options_description opt("./sim option");
	opt.add_options()
        ("help,h", "show help")
        ("file,f", po::value<std::string>(), "filename")
        ("bin,b", "binary-input mode")
        ("debug,d", "debug mode")
        ("info,i", "information-output mode")
        ("mem,m", po::value<int>(), "memory size")
        ("ieee", "IEEE754 mode")
        ("skip,s", "skipping bootloading")
        ("preload", po::value<std::string>()->implicit_value("contest"), "data preload")
        ("raytracing,r", "specialized for ray-tracing program")
        #ifdef DETAILED
        ("port,p", po::value<int>(), "port number")
        ("boot", "bootloading mode")
        ("cache,c", po::value<std::vector<unsigned int>>()->multitoken(), "cache setting")
        ("gshare,g", "branch prediction (Gshare)")
        ("stat", "statistics mode")
        ("cautious", "cautious mode")
        #endif
        ;
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
    if(vm.count("bin")) is_bin = true;
    if(vm.count("debug")) is_debug = true;
    if(vm.count("info")) is_info_output = true;
    if(vm.count("mem")) mem_size = vm["mem"].as<int>();
    if(vm.count("ieee")) is_ieee = true;
    if(vm.count("skip")) is_skip = true;
    if(vm.count("preload")){
        is_preloading = true;
        preload_filename = vm["preload"].as<std::string>();
    };
    if(vm.count("raytracing")) is_raytracing = true;
    #ifdef DETAILED
    if(vm.count("port")) port = vm["port"].as<int>();
    if(vm.count("boot")) is_bootloading = true;
    if(vm.count("cache")){
        is_cache_enabled = true;
        std::vector<unsigned int> cache_setting = vm["cache"].as<std::vector<unsigned int>>();
        if(cache_setting.size() == 2){
            index_width = cache_setting[0];
            offset_width = cache_setting[1];
            if(index_width + offset_width >= 32){
                std::cout << head_error << "invalid cache setting" << std::endl;
                std::exit(EXIT_FAILURE);
            }
        }else{
            std::cout << head_error << "invalid argument(s) for -c option (there should be 2 ones)" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
    if(vm.count("gshare")) is_gshare_enabled = true;
    if(vm.count("cautious")) is_cautious = true;
    if(vm.count("stat")){
        is_stat = true;
    }
    #endif

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
    std::cout << head << "simulation start" << std::endl;
    auto start = std::chrono::system_clock::now();

    // ブートローダ処理をスキップする場合の処理
    if(is_skip){
        op_list.resize(100);
        pc = 100;
    }

    // レイトレを処理する場合の特別対応
    if(is_raytracing){
        // 命令用のvectorを確保
        op_list.reserve(12000);

        // メモリはこれくらい
        mem_size = 2500000; // 10MB

        // バッファ先読みを有効に
        is_preloading = true;
        preload_filename = "contest";
    }

    // メモリ領域の確保
    memory = Memory(mem_size);

    // キャッシュの初期化
    cache = Cache(index_width, offset_width);

    // 統計データの初期化
    if(is_stat){
        mem_accessed_read = (unsigned long long*) calloc(mem_size, sizeof(unsigned long long));
        mem_accessed_write = (unsigned long long*) calloc(mem_size, sizeof(unsigned long long));
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

    code_size = code_id;

    if(is_stat) line_exec_count = (unsigned int*) calloc(input_line_num, sizeof(unsigned int));

    auto end = std::chrono::system_clock::now();
    auto msec = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << head << "time elapsed (preparation): " << msec << std::endl;

    #ifdef DETAILED
    // コマンドの受け付けとデータ受信処理を別々のスレッドで起動
    std::thread t1(simulate);
    std::thread t2(receive_data);
    Cancel_flag flg;
    std::thread t3(send_data, std::ref(flg));
    t1.join();
    t2.detach();
    flg.signal();
    t3.join();
    #else
    simulate();
    #endif

    // 実行結果の情報を出力
    if(is_info_output || is_stat) output_info();
    
    // レイトレの場合は画像も出力
    if(is_raytracing && sim_state == sim_state_end){
        if(!send_buffer.empty()){
            std::string output_filename = "./out/output_" + timestamp + ".ppm";
            std::ofstream output_file(output_filename);
            if(!output_file){
                std::cerr << head_error << "could not open " << output_filename << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::stringstream output;
            while(!send_buffer.empty()){
                output << (unsigned char) send_buffer.pop().i;
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
    try{
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
    }catch(std::exception& e){
        exit_with_output(e);
    }
}

// デバッグモードのコマンドを認識して実行
bool is_in_step = false; // step実行の途中
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
        if(sim_state != sim_state_end){
            std::cout << "pc " << pc << " (line " << id_to_line.left.at(pc) << ") " << op_list[pc].to_string() << std::endl;
            if((sim_state = exec_op()) == sim_state_end){
                std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(do))\\s+(\\d+)\\s*$"))){ // do N
        unsigned int n = std::stoi(match[3].str());
        if(sim_state != sim_state_end){
            for(unsigned int i=0; i<n; ++i){
                if((sim_state = exec_op()) == sim_state_end){
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(u|(until))\\s+(\\d+)\\s*$"))){ // until N
        unsigned int n = std::stoi(match[3].str());
        if(sim_state != sim_state_end){
            while(op_count() < n){
                if((sim_state = exec_op()) == sim_state_end){
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
            if(sim_state != sim_state_end) std::cout << head_info << "executed " << n << " operations" << std::endl;
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(s|(step))\\s*$"))){ // step
        if(sim_state != sim_state_end){
            if(op_list[pc].type == o_jalr || op_list[pc].type == o_jal){
                int old_pc = pc;
                is_in_step = true;
                exec_command("do");
                if(sim_state != sim_state_end){
                    exec_command("break " + std::to_string(id_to_line.left.at(old_pc + 1)) + " __ret");
                    exec_command("continue __ret");
                    exec_command("delete __ret");
                    std::cout << head_info << "step execution around pc " << old_pc << " (line " << id_to_line.left.at(old_pc) << ") " << op_list[old_pc].to_string() << std::endl;
                }
                is_in_step = false;
            }else{
                exec_command("do");
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(r|(run))(\\s+(-t))?\\s*$"))){ // run
        bool is_time_measuring = match[4].str() == "-t";
        auto start = std::chrono::system_clock::now();

        // Endになるまで実行
        while((sim_state = exec_op()) != sim_state_end);
        auto end = std::chrono::system_clock::now();
        std::cout << head_info << "all operations have been simulated successfully!" << std::endl;

        // 実行時間などの表示
        if(is_time_measuring){
            exec_time = std::chrono::duration<double>(end - start).count();
            std::cout << head << "time elapsed (execution): " << exec_time << std::endl;
            unsigned long long cnt = op_count();
            std::cout << head << "operation count: " << cnt << std::endl;
            op_per_sec = static_cast<double>(cnt) / exec_time;
            std::cout << head << "operations per second: " << op_per_sec << std::endl;
        }
        // メモリ使用量を保存しておく
        if(is_raytracing){
            memory_used = reg_int.read_int(3);
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(init))\\s*$"))){ // init
        breakpoint_skip = false;
        simulation_end = false;
        pc = is_skip ? 100 : 0; // PCを0にする
        for(unsigned int i=0; i<op_type_num; ++i) op_type_count[i] = 0;
        reg_int = Reg(); // レジスタをクリア
        reg_fp = Reg();
        memory = Memory(mem_size);
        std::cout << head_info << "simulation environment is now initialized" << std::endl;
    }else if(std::regex_match(cmd, std::regex("^\\s*(ir|(init run))\\s*$"))){ // init run
        exec_command("init");
        exec_command("run");
    }else if(std::regex_match(cmd, std::regex("^\\s*(c|(continue))\\s*$"))){ // continue
        if(sim_state != sim_state_end){
            while(true){
                switch(sim_state = exec_op("__continue")){
                    case sim_state_continue: break;
                    case sim_state_end:
                        std::cout << head_info << "all operations have been simulated successfully! (no breakpoint encountered)" << std::endl;
                        break;
                    default:
                        if(sim_state >= 0){ // ブレークポイントに当たった
                            std::cout << head_info << "halt before breakpoint '" + bp_to_id.right.at(sim_state) << "' (pc " << sim_state << ", line " << id_to_line.left.at(sim_state) << ")" << std::endl;
                        }else{
                            throw std::runtime_error("invalid response from exec_op");
                        }
                }
                if(sim_state != sim_state_continue) break;
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(c|(continue))\\s+(([a-zA-Z_]\\w*(.\\d+)*))\\s*$"))){ // continue break
        if(sim_state != sim_state_end){
            std::string bp = match[3].str();
            if(bp_to_id.left.find(bp) != bp_to_id.left.end()){
                while(true){
                    switch(sim_state = exec_op(bp)){
                        case sim_state_continue: break;
                        case sim_state_end:
                            std::cout << head_info << "all operations have been simulated successfully! (breakpoint '" << bp << "' not encountered)"  << std::endl;
                            break;
                        default:
                            if(sim_state >= 0){ // ブレークポイントに当たった
                                if(!is_in_step) std::cout << head_info << "halt before breakpoint '" + bp << "' (pc " << sim_state << ", line " << id_to_line.left.at(sim_state) << ")" << std::endl;
                            }else{
                                throw std::runtime_error("invalid response from exec_op");
                            }
                    }
                    if(sim_state != sim_state_continue) break;
                }
            }else{
                std::cout << head_error << "breakpoint '" << bp << "' has not been set" << std::endl;
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(info))\\s*$"))){ // info
        std::cout << "operations executed: " << op_count() << std::endl;
        if(simulation_end){
            std::cout << "next: (no operation left to be simulated)" << std::endl;
        }else{
            std::cout << "next: pc " << pc << " (line " << id_to_line.left.at(pc) << ") " << op_list[pc].to_string() << std::endl;
        }
        if(bp_to_id.empty()){
            std::cout << "breakpoints: (no breakpoint found)" << std::endl;
        }else{
            std::cout << "breakpoints:" << std::endl;
            for(auto x : bp_to_id.left) {
                std::cout << "  " << x.first << " (pc " << x.second << ", line " << id_to_line.left.at(x.second) << ")" << std::endl;
            }
        }
        if(is_gshare_enabled){
            std::cout << "prediction stat:" << std::endl;
            std::cout << "  taken rate: " << static_cast<double>(branch_predictor.taken_count) / branch_predictor.total_count << std::endl;
            std::cout << "  correct rate: " << static_cast<double>(branch_predictor.correct_count) / branch_predictor.total_count << std::endl;
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(p|(print))\\s+reg\\s*$"))){ // print reg
        reg_int.print(true, t_default);
        reg_fp.print(false, t_float);
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))\\s+(rbuf|sbuf)(\\s+(\\d+))?\\s*$"))){ // print rbuf/sbuf N
        unsigned int size;
        if(match[4].str() == ""){
            size = 10; // default
        }else{
            size = std::stoi(match[4].str());
        }

        if(match[3].str() == "rbuf"){
            if(receive_buffer.empty()){
                std::cout << "receive buffer: (empty)" << std::endl;
            }else{
                std::cout << "receive buffer:\n  ";
                receive_buffer.print(size);
            }
        }else if(match[3].str() == "sbuf"){
            if(send_buffer.empty()){
                std::cout << "send buffer: (empty)" << std::endl;
            }else{
                std::cout << "send buffer:\n  ";
                send_buffer.print(size);
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))(\\s+-(d|b|h|f|o))?(\\s+(x|f)(\\d+))+\\s*$"))){ // print (option) reg
        int reg_no;
        Stype st = t_default;
        char option = match[4].str().front();
        switch(option){
            case 'd': st = t_dec; break;
            case 'b': st = t_bin; break;
            case 'h': st = t_hex; break;
            case 'f': st = t_float; break;
            case 'o': st = t_op; break;
            default: break;
        }

        while(std::regex_search(cmd, match, std::regex("(x|f)(\\d+)"))){
            reg_no = std::stoi(match[2].str());
            if(match[1].str() == "x"){ // int
                std::cout << "\x1b[1m%x" << reg_no << "\x1b[0m: " << reg_int.read_32(reg_no).to_string(st) << std::endl;
            }else{ // float
                if(st == t_default) st = t_float; // デフォルトはfloat
                std::cout << "\x1b[1m%f" << reg_no << "\x1b[0m: " << reg_fp.read_32(reg_no).to_string(st) << std::endl;
            }
            cmd = match.suffix();
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))(\\s+(-w))?\\s+(m|mem)\\[(\\d+):(\\d+)\\]\\s*$"))){ // print mem[N:M]
        int start = std::stoi(match[6].str());
        int width = std::stoi(match[7].str());
        memory.print(start, width);
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(s|(set))\\s+(x(\\d+))\\s+(\\d+)\\s*$"))){ // set reg N
        int reg_no = std::stoi(match[4].str());
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
                if(label_to_id.right.find(id) == label_to_id.right.end() || is_in_step){ // idにはラベルが付いていない？
                    if(bp_to_id.left.find(bp) == bp_to_id.left.end()){ // そのブレークポイント名は使われていない？
                        if(label_to_id.left.find(bp) == label_to_id.left.end()){ // そのブレークポイント名はラベル名と重複していない？
                            bp_to_id.insert(bimap_value_t(bp, id));
                            if(!is_in_step) std::cout << head_info << "breakpoint '" << bp << "' is now set to line " << line_no << std::endl;
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
                std::cout << head_info << "breakpoint '" << label << "' is now set (at pc " << label_id << ", line " << id_to_line.left.at(label_id) << ")" << std::endl;
            
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
            if(!is_in_step){
                std::cout << head_info << "breakpoint '" << bp_id << "' is now deleted" << std::endl;
            }
        }else{
            std::cout << head_error << "breakpoint '" << bp_id << "' has not been set" << std::endl;  
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(out)(\\s+(-p|-b))?(\\s+(-f)\\s+(\\w+))?\\s*$"))){ // out (option)
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
            TransmissionQueue copy = send_buffer;
            if(is_ppm){
                while(!copy.empty()){
                    output << (unsigned char) copy.pop().i;
                }
            }else if(is_bin){
                unsigned int i;
                while(!copy.empty()){
                    i = copy.pop().i;
                    output.write((char*) &i, sizeof(char)); // 8bitだけ書き込む
                }
            }else{
                while(!copy.empty()){
                    output << copy.pop().to_string(t_hex) << std::endl;
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
int exec_op(){
    Operation op = op_list[pc];
    
    // 統計モードの場合、行数ごとの実行回数を更新
    #ifdef DETAILED
    if(is_stat){
        int l = id_to_line.left.at(pc);
        if(l > 0) ++line_exec_count[l-1];
    }
    #endif

    // ブートローダ用処理(bootloader.sの内容に依存しているので注意！)
    #ifdef DETAILED
    if(is_bootloading){
        if(op.type == o_std && op.rs2 == 5){ // std %x5
            int x5 = reg_int.read_int(5);
            if(x5 == 153){ // 0x99
                is_waiting_for_lnum = true;
            }else if(x5 == 170){ // 0xaa
                is_loading_codes = false;
                is_bootloading = false; // ブートローダ用処理の終了
                if(is_debug){
                    id_to_line = std::move(id_to_line_loaded);
                    label_to_id = std::move(label_to_id_loaded);
                    bp_to_id = std::move(bp_to_id_loaded);
                    TransmissionQueue new_buf;
                    receive_buffer = new_buf;
                    std::cout << head_info << "bootloading end" << std::endl;
                }
            }
        }

        if(is_waiting_for_lnum && op.type == o_addi && op.rs1 == 6 && op.rd == 7 && op.imm == 0){ // addi %x7, %x6, 0
            is_waiting_for_lnum = false;
            int loaded_op_num = reg_int.read_int(6);
            if(is_debug){
                std::cout << head_info << "operations to be loaded: " << loaded_op_num << std::endl;
                is_loading_codes = true; // 命令のロード開始
            }
            op_list.resize(100 + loaded_op_num); // 受け取る命令の数に合わせてop_listを拡大
        }
    }
    #endif

    // レイトレに対する無限ループ検知
    #ifdef DETAILED
    if(is_raytracing && is_cautious && op_count() >= max_op_count){
        throw std::runtime_error("too many operations executed for raytracing program");
    }
    #endif

    // 実行部分
    switch(op.type){
        case o_add:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) + reg_int.read_int(op.rs2));
            ++op_type_count[o_add];
            ++pc;
            break;
        case o_sub:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) - reg_int.read_int(op.rs2));
            ++op_type_count[o_sub];
            ++pc;
            break;
        case o_sll:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) << reg_int.read_int(op.rs2));
            ++op_type_count[o_sll];
            ++pc;
            break;
        case o_srl:
            reg_int.write_int(op.rd, static_cast<unsigned int>(reg_int.read_int(op.rs1)) >> reg_int.read_int(op.rs2));
            ++op_type_count[o_srl];
            ++pc;
            break;
        case o_sra:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) >> reg_int.read_int(op.rs2)); // note: 処理系依存
            ++op_type_count[o_sra];
            ++pc;
            break;
        case o_and:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) & reg_int.read_int(op.rs2));
            ++op_type_count[o_and];
            ++pc;
            break;
        case o_fabs:
            if(is_ieee){
                reg_fp.write_float(op.rd, std::abs(reg_fp.read_float(op.rs1)));
            }else{
                reg_fp.write_32(op.rd, fpu.fabs(reg_fp.read_32(op.rs1)));
            }
            ++op_type_count[Otype::o_fabs];
            ++pc;
            break;
        case o_fneg:
            if(is_ieee){
                reg_fp.write_float(op.rd, - reg_fp.read_float(op.rs1));
            }else{
                reg_fp.write_32(op.rd, fpu.fneg(reg_fp.read_32(op.rs1)));
            }
            ++op_type_count[Otype::o_fneg];
            ++pc;
            break;
        case o_fdiv:
            if(is_ieee){
                reg_fp.write_float(op.rd, reg_fp.read_float(op.rs1) / reg_fp.read_float(op.rs2));
            }else{
                reg_fp.write_32(op.rd, fpu.fdiv(reg_fp.read_32(op.rs1), reg_fp.read_32(op.rs2)));
            }
            ++op_type_count[o_fdiv];
            ++pc;
            break;
        case o_fsqrt:
            if(is_ieee){
                reg_fp.write_float(op.rd, std::sqrt(reg_fp.read_float(op.rs1)));
            }else{
                reg_fp.write_32(op.rd, fpu.fsqrt(reg_fp.read_32(op.rs1)));
            }
            ++op_type_count[o_fsqrt];
            ++pc;
            break;
        case o_fcvtif:
            if(is_ieee){
                reg_fp.write_float(op.rd, static_cast<float>(reg_fp.read_int(op.rs1)));
            }else{
                reg_fp.write_32(op.rd, fpu.itof(reg_fp.read_32(op.rs1)));
            }
            ++op_type_count[o_fcvtif];
            ++pc;
            break;
        case o_fcvtfi:
            if(is_ieee){
                reg_fp.write_float(op.rd, static_cast<int>(std::nearbyint(reg_fp.read_float(op.rs1))));
            }else{
                reg_fp.write_32(op.rd, fpu.ftoi(reg_fp.read_32(op.rs1)));
            }
            ++op_type_count[o_fcvtfi];
            ++pc;
            break;
        case o_fmvff:
            reg_fp.write_32(op.rd, reg_fp.read_32(op.rs1));
            ++op_type_count[o_fmvff];
            ++pc;
            break;
        case o_fadd:
            if(is_ieee){
                reg_fp.write_float(op.rd, reg_fp.read_float(op.rs1) + reg_fp.read_float(op.rs2));
            }else{
                reg_fp.write_32(op.rd, fpu.fadd(reg_fp.read_32(op.rs1), reg_fp.read_32(op.rs2)));
            }
            ++op_type_count[o_fadd];
            ++pc;
            break;
        case o_fsub:
            if(is_ieee){
                reg_fp.write_float(op.rd, reg_fp.read_float(op.rs1) - reg_fp.read_float(op.rs2));
            }else{
                reg_fp.write_32(op.rd, fpu.fsub(reg_fp.read_32(op.rs1), reg_fp.read_32(op.rs2)));
            }
            ++op_type_count[o_fsub];
            ++pc;
            break;
        case o_fmul:
            if(is_ieee){
                reg_fp.write_float(op.rd, reg_fp.read_float(op.rs1) * reg_fp.read_float(op.rs2));
            }else{
                reg_fp.write_32(op.rd, fpu.fmul(reg_fp.read_32(op.rs1), reg_fp.read_32(op.rs2)));
            }
            ++op_type_count[o_fmul];
            ++pc;
            break;
        case o_beq:
            reg_int.read_int(op.rs1) == reg_int.read_int(op.rs2) ? pc += op.imm : ++pc;
            ++op_type_count[o_beq];
            #ifdef DETAILED
            branch_predictor.update(pc, reg_int.read_int(op.rs1) == reg_int.read_int(op.rs2));
            #endif
            break;
        case o_blt:
            reg_int.read_int(op.rs1) < reg_int.read_int(op.rs2) ? pc += op.imm : ++pc;
            ++op_type_count[o_blt];
            #ifdef DETAILED
            branch_predictor.update(pc, reg_int.read_int(op.rs1) < reg_int.read_int(op.rs2));
            #endif
            break;
        case o_fbeq:
            reg_fp.read_float(op.rs1) == reg_fp.read_float(op.rs2) ? pc += op.imm : ++pc;
            ++op_type_count[o_fbeq];
            #ifdef DETAILED
            branch_predictor.update(pc, reg_fp.read_float(op.rs1) == reg_fp.read_float(op.rs2));
            #endif
            break;
        case o_fblt:
            reg_fp.read_float(op.rs1) < reg_fp.read_float(op.rs2) ? pc += op.imm : ++pc;
            ++op_type_count[o_fblt];
            #ifdef DETAILED
            branch_predictor.update(pc, reg_fp.read_float(op.rs1) < reg_fp.read_float(op.rs2));
            #endif
            break;
        case o_sw:
            write_memory(reg_int.read_int(op.rs1) + op.imm, reg_int.read_32(op.rs2));
            ++op_type_count[o_sw];
            ++pc;
            break;
        case o_si:
            op_list[reg_int.read_int(op.rs1) + op.imm] = Operation(reg_int.read_int(op.rs2));
            ++op_type_count[o_si];
            ++pc;
            break;
        case o_std:
            send_buffer.push(reg_int.read_int(op.rs2));
            ++op_type_count[o_std];
            ++pc;
            break;
        case o_fsw:
            write_memory(reg_int.read_int(op.rs1) + op.imm, reg_fp.read_32(op.rs2));
            ++op_type_count[o_fsw];
            ++pc;
            break;
        case o_addi:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) + op.imm);
            ++op_type_count[o_addi];
            ++pc;
            break;
        case o_slli:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) << op.imm);
            ++op_type_count[o_slli];
            ++pc;
            break;
        case o_srli:
            reg_int.write_int(op.rd, static_cast<unsigned int>(reg_int.read_int(op.rs1)) >> op.imm);
            ++op_type_count[o_srli];
            ++pc;
            break;
        case o_srai:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) >> op.imm); // todo: 処理系依存
            ++op_type_count[o_srai];
            ++pc;
            break;
        case o_andi:
            reg_int.write_int(op.rd, reg_int.read_int(op.rs1) & op.imm);
            ++op_type_count[o_andi];
            ++pc;
            break;
        case o_lw:
            reg_int.write_32(op.rd, read_memory(reg_int.read_int(op.rs1) + op.imm));
            ++op_type_count[o_lw];
            ++pc;
            break;
        case o_lre:
            reg_int.write_int(op.rd, receive_buffer.empty() ? 1 : 0);
            ++pc;
            ++op_type_count[o_lre];
            break;
        case o_lrd:
            if(!receive_buffer.empty()){
                reg_int.write_32(op.rd, receive_buffer.pop());
            }else{
                throw std::runtime_error("receive buffer is empty [lrd] (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(pc))) : "") + ")");
            }
            ++op_type_count[o_lrd];
            ++pc;
            break;
        case o_ltf:
            reg_int.write_int(op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
            ++op_type_count[o_ltf];
            ++pc;
            break;
        case o_flw:
            reg_fp.write_32(op.rd, read_memory(reg_int.read_int(op.rs1) + op.imm));
            ++op_type_count[o_flw];
            ++pc;
            break;
        case o_jalr:
            {
                unsigned next_pc = pc + 1;
                pc = reg_int.read_int(op.rs1);
                reg_int.write_int(op.rd, next_pc);
                ++op_type_count[o_jalr];
            }
            break;
        case o_jal:
            reg_int.write_int(op.rd, pc + 1);
            ++op_type_count[o_jal];
            pc += op.imm;
            break;
        case o_lui:
            reg_int.write_int(op.rd, op.imm << 12);
            ++op_type_count[o_lui];
            ++pc;
            break;
        case o_fmvif:
            reg_fp.write_32(op.rd, reg_int.read_32(op.rs1));
            ++op_type_count[o_fmvif];
            ++pc;
            break;
        case o_fmvfi:
            reg_int.write_32(op.rd, reg_fp.read_32(op.rs1));
            ++op_type_count[o_fmvfi];
            ++pc;
            break;
        default:
            throw std::runtime_error("error in executing the code (at pc " + std::to_string(pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(pc))) : "") + ")");
    }

    return (pc >= code_size || op.is_exit()) ? sim_state_end : sim_state_continue;
}

int exec_op(const std::string& bp){
    int res = exec_op();

    if(is_debug && bp != ""){
        if(bp == "__continue"){ // continue, 名前指定なし
            if(bp_to_id.right.find(pc) != bp_to_id.right.end()) res = pc;
        }else{ // continue, 名前指定あり
            unsigned int bp_id = bp_to_id.left.at(bp);
            if(pc == bp_id) res = pc;
        }
    }

    return res;
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
    ss << "# execution stat" << std::endl;
    ss << "- execution time(s): " << exec_time << std::endl;
    ss << "- operations per second: " << op_per_sec << std::endl;
    ss << std::endl;

    ss << "# basic stat" << std::endl;
    ss << "- operation count: " << op_count() << std::endl;
    if(is_cache_enabled){
        ss << "- cache:" << std::endl;
        ss << "\t- line num: " << std::pow(2, index_width) << std::endl;
        ss << "\t- block size: " << std::pow(2, offset_width) << std::endl;
        ss << "\t- accessed: " << cache.read_times << std::endl;
        ss << "\t- hit: " << cache.hit_times << std::endl;
        ss << "\t- hit rate: " << static_cast<double>(cache.hit_times) / cache.read_times << std::endl;
    }
    if(is_stat && is_raytracing){
        ss << "- stack:" << std::endl;
        ss << "\t- size: " << max_x2 << std::endl;
        ss << "\t- read: " << stack_accessed_read_count << std::endl;
        ss << "\t- write: " << stack_accessed_write_count << std::endl;
        ss << "- heap: " << std::endl;
        ss << "\t- size: " << memory_used - stack_border << std::endl;
        ss << "\t- read: " << heap_accessed_read_count << std::endl;
        ss << "\t- write: " << heap_accessed_write_count << std::endl;
    }
    if(is_gshare_enabled){
        ss << "- branch prediction:" << std::endl;
        ss << "\t- taken rate: " << static_cast<double>(branch_predictor.taken_count) / branch_predictor.total_count << std::endl;
        ss << "\t- correct rate: " << static_cast<double>(branch_predictor.correct_count) / branch_predictor.total_count << std::endl;
    }
    ss << std::endl;
    ss << "# operation stat" << std::endl;
    for(int i=0; i<op_type_num; ++i){
        ss << "- " << string_of_otype(static_cast<Otype>(i)) << ": " << op_type_count[i] << std::endl;
    }
    output_file << ss.str();
    std::cout << head << "simulation stat: " << output_filename << std::endl;

    if(is_stat){
        // メモリの情報
        std::string output_filename_mem = "./info/" + filename + "_mem_" + timestamp + ".csv";
        std::ofstream output_file_mem(output_filename_mem);
        if(!output_file_mem){
            std::cerr << head_error << "could not open " << output_filename_mem << std::endl;
            std::exit(EXIT_FAILURE);
        }
        std::stringstream ss_mem;
        int m = is_raytracing ? memory_used : mem_size;
        ss_mem << "address,value,read,write" << std::endl;
        for(int i=0; i<m; ++i){
            ss_mem << i << "," << memory.read(i).to_string(t_hex) << "," << mem_accessed_read[i] << "," << mem_accessed_write[i] << std::endl;
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

inline Bit32 read_memory(int w){
    #ifdef DETAILED
    if(is_cautious){
        if(w < 0 || w > memory_border) throw std::runtime_error("invalid memory access");
    }
    if(is_stat){
        ++mem_accessed_read[w];
        if(is_raytracing) w < stack_border ? ++stack_accessed_read_count : ++heap_accessed_read_count;
    }
    if(is_cache_enabled) cache.read(w);
    #endif
    return memory.read(w);
}

inline void write_memory(int w, const Bit32& v){
    #ifdef DETAILED
    if(is_cautious){
        if(w < 0 || w > memory_border) throw std::runtime_error("invalid memory access");
    }
    if(is_stat){
        ++mem_accessed_write[w];
        if(is_raytracing) w < stack_border ? ++stack_accessed_write_count : ++heap_accessed_write_count;
    }
    if(is_cache_enabled) cache.write(w);
    #endif
    memory.write(w, v);
}

// 実行命令の総数を返す
unsigned long long op_count(){
    unsigned long long acc = 0;
    for(unsigned int i=0; i<op_type_num; ++i){
        acc += op_type_count[i];
    }
    return acc;
}

// 実行情報を表示したうえで異常終了
void exit_with_output(std::exception& e){
    std::cout << head_error << e.what() << std::endl;
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
