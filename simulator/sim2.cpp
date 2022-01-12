#include <sim2.hpp>
#include <common.hpp>
#include <fpu.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <optional>
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
#include <nameof.hpp>

namespace po = boost::program_options;

/* グローバル変数 */
// 内部処理関係
Configuration config; // 各時点の状態
std::vector<Operation> op_list; // 命令のリスト(PC順)
Reg reg_int; // 整数レジスタ
Reg reg_fp; // 浮動小数点数レジスタ
Bit32 *memory; // メモリ領域
unsigned int code_size = 0; // コードサイズ

int mem_size = 100; // メモリサイズ
constexpr unsigned long long max_op_count = 10000000000;

int port = 20214; // 通信に使うポート番号
std::queue<Bit32> receive_buffer; // 外部通信での受信バッファ
boost::lockfree::queue<Bit32> send_buffer(3*1e6); // 外部通信での受信バッファ

// シミュレーションの制御
bool is_debug = false; // デバッグモード
bool is_bin = false; // バイナリファイルモード
bool is_raytracing = false; // レイトレ専用モード
bool is_skip = false; // ブートローディングの過程をスキップするモード
bool is_quick = false; // マルチサイクルの処理をすぐに終わったものと見なすモード
bool is_ieee = false; // IEEE754に従って浮動小数演算を行うモード
bool is_preloading = false; // バッファのデータを予め取得しておくモード
std::string filename; // 処理対象のファイル名
std::string preload_filename; // プリロード対象のファイル名
unsigned int bp_counter = 0; // ブレークポイント自動命名のときに使う数字

// 統計・出力関連
unsigned long long *op_type_count; // 各命令の実行数
std::string timestamp; // ファイル出力の際に使うタイムスタンプ

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
        ("port,p", po::value<int>(), "port number")
        ("mem,m", po::value<int>(), "memory size")
        ("raytracing,r", "specialized for ray-tracing program")
        ("skip,s", "skipping bootloading")
        ("quick,q", "quick multicycle mode")
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
    if(vm.count("mem")) mem_size = vm["mem"].as<int>();
    if(vm.count("raytracing")) is_raytracing = true;
    if(vm.count("skip")) is_skip = true;
    if(vm.count("quick")) is_quick = true;
    if(vm.count("ieee")) is_ieee = true;
    if(vm.count("preload")){
        is_preloading = true;
        preload_filename = vm["preload"].as<std::string>();
    };

    // 命令数カウントの初期化
    op_type_count = (unsigned long long*) calloc(op_type_num, sizeof(unsigned long long));

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

    // ブートローダ処理をスキップする場合の処理
    if(is_skip){
        op_list.resize(100);
        config.IF.pc[0] = 392;
        config.IF.pc[0] = 396;
    }

    // レイトレを処理する場合は予めreserve
    if(is_raytracing){
        op_list.reserve(12000);
        mem_size = 2500000; // 10MB
    }

    // メモリ領域の確保
    memory = (Bit32*) calloc(mem_size, sizeof(Bit32));

    // RAMの初期化
    init_ram();

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
    input_filename = "./code/" + filename + (is_bin ? ".bin" : (is_debug ? ".dbg" : ""));
    std::ifstream input_file(input_filename);
    if(!input_file.is_open()){
        std::cerr << head_error << "could not open " << input_filename << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // ファイルの各行をパースしてop_listに追加
    std::string code;
    std::string code_keep; // 空白でない最後の行を保存
    unsigned int code_id = 0; // is_skip ? 100 : 0;
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

    code_size = code_id;
    op_list.resize(code_id + 2); // segmentation fault防止のために余裕を持たせる

    // コマンドの受け付けとデータ受信処理を別々のスレッドで起動
    std::thread t1(simulate);
    std::thread t2(receive_data);
    Cancel_flag flg;
    std::thread t3(send_data, std::ref(flg));
    t1.join();
    t2.detach();
    flg.signal();
    t3.join();

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
    }else{ // デバッグなしモード
        exec_command("run -t");
    }

    return;
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

    while(true){
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_size);
        recv(client_socket, buf, 32, 0);
        std::string data(buf);
        memset(buf, '\0', sizeof(buf)); // バッファをクリア
        for(int i=0; i<4; ++i){ // big endianで受信したものとしてバッファに追加
            receive_buffer.push(bit32_of_data(data.substr(i * 8, 8)));
        }
    }
}

// データの送信
void send_data(Cancel_flag& flg){
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

// デバッグモードのコマンドを認識して実行
bool no_info = false; // infoを表示しない(一時的な仕様)
int sim_state = sim_state_continue; // シミュレータの状態管理
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
            if((sim_state = config.advance_clock(true, "")) == sim_state_end){
                std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(d|(do))\\s+(\\d+)\\s*$"))){ // do N
        unsigned int n = std::stoi(match[3].str());
        if(sim_state != sim_state_end){
            for(unsigned int i=0; i<n; ++i){
                if((sim_state = config.advance_clock(false, "")) == sim_state_end){
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(s|(step))\\s*$"))){ // step
        if(sim_state != sim_state_end){
            if(config.EX.br.inst.op.type == Otype::o_jalr || config.EX.br.inst.op.type == Otype::o_jal){
                int old_pc = config.EX.br.inst.pc;
                no_info = true;
                exec_command("do");
                exec_command("break " + std::to_string(id_to_line.left.at(id_of_pc(old_pc + 4))) + " __ret");
                exec_command("continue __ret");
                exec_command("delete __ret");
                no_info = false;
                std::cout << head_info << "step execution around pc " << old_pc << " (line " << id_to_line.left.at(id_of_pc(old_pc)) << ") " << op_list[id_of_pc(old_pc)].to_string() << std::endl;
            }else{
                exec_command("do");
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(r|(run))(\\s+(-t))?\\s*$"))){ // run
        if(sim_state != sim_state_end){
            bool is_time_measuring = match[4].str() == "-t";
            auto start = std::chrono::system_clock::now();
            // Endになるまで実行
            while((sim_state = config.advance_clock(false, "")) != sim_state_end);
            std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
            // 実行時間などの情報の表示
            if(is_time_measuring){
                auto end = std::chrono::system_clock::now();
                double exec_time = std::chrono::duration<double>(end - start).count();
                std::cout << head << "time elapsed (execution): " << exec_time << std::endl;
                unsigned long long cnt = op_count();
                std::cout << head << "operation count: " << cnt << std::endl;
                double op_per_sec = static_cast<double>(cnt) / exec_time;
                std::cout << head << "operations per second: " << op_per_sec << std::endl;
                std::cout << head << "clock count: " << config.clk << std::endl;
                std::cout << head << "operations per clock: " << static_cast<double>(cnt) / config.clk << std::endl;
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(init))\\s*$"))){ // init
        sim_state = sim_state_continue;
        config = Configuration();
        for(unsigned int i=0; i<op_type_num; ++i){
            op_type_count[i] = 0;
        }
        reg_int = Reg();
        reg_fp = Reg();
        for(int i=0; i<mem_size; ++i){
            memory[i] = Bit32(0);
        }
        std::cout << head_info << "simulation environment is now initialized" << std::endl;
    }else if(std::regex_match(cmd, std::regex("^\\s*(ir|(init run))\\s*$"))){ // init run
        exec_command("init");
        exec_command("run");
    }else if(std::regex_match(cmd, std::regex("^\\s*(c|(continue))\\s*$"))){ // continue
        if(sim_state != sim_state_end){
            while(true){
                switch(sim_state = config.advance_clock(false, "__continue")){
                    case sim_state_continue: break;
                    case sim_state_end:
                        std::cout << head_info << "all operations have been simulated successfully! (no breakpoint encountered)" << std::endl;
                        break;
                    default:
                        if(sim_state >= 0){ // ブレークポイントに当たった
                            std::cout << head_info << "halt before breakpoint '" + bp_to_id.right.at(id_of_pc(sim_state)) << "' (pc " << sim_state << ", line " << id_to_line.left.at(id_of_pc(sim_state)) << ")" << std::endl;
                        }else{
                            std::cout << head_error << "invalid response from Configuration::advance_clock" << std::endl;
                            std::exit(EXIT_FAILURE);
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
                    switch(sim_state = config.advance_clock(false, bp)){
                        case sim_state_continue: break;
                        case sim_state_end:
                            std::cout << head_info << "all operations have been simulated successfully! (breakpoint '" << bp << "' not encountered)"  << std::endl;
                            break;
                        default:
                            if(sim_state >= 0){ // ブレークポイントに当たった
                                if(!no_info){
                                    std::cout << head_info << "halt before breakpoint '" + bp << "' (pc " << sim_state << ", line " << id_to_line.left.at(id_of_pc(sim_state)) << ")" << std::endl;
                                }
                            }else{
                                std::cout << head_error << "invalid response from Configuration::advance_clock" << std::endl;
                                std::exit(EXIT_FAILURE);
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
        if(sim_state == sim_state_end){
            std::cout << "simulation state: (no operation left to be simulated)" << std::endl;
        }else{
            // todo
        }
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
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))\\s+buf(\\s+(\\d+))?\\s*$"))){ // print buf
        if(receive_buffer.empty()){
            std::cout << "receive buffer:" << std::endl;
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
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(b|(break))\\s+(\\d+)\\s*$"))){ // break N (Nはアセンブリコードの行数)
        unsigned int line_no = std::stoi(match[3].str());
        if(id_to_line.right.find(line_no) != id_to_line.right.end()){ // 行番号は命令に対応している？
            unsigned int id = id_to_line.right.at(line_no);
            if(bp_to_id.right.find(id) == bp_to_id.right.end()){ // idはまだブレークポイントが付いていない？
                if(label_to_id.right.find(id) == label_to_id.right.end()){ // idにはラベルが付いていない？
                    bp_to_id.insert(bimap_value_t("__bp" + std::to_string(bp_counter), id));
                    std::cout << head_info << "breakpoint '" << ("__bp" + std::to_string(bp_counter)) << "' is now set to line " << line_no << std::endl;
                    ++bp_counter;
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

// キューの表示
void print_queue(std::queue<Bit32> q, int n){
    while(!q.empty() && n > 0){
        std::cout << q.front().to_string() << "; ";
        q.pop();
        n--;
    }
    std::cout << std::endl;
}

// 実効命令の総数を返す
unsigned long long op_count(){
    unsigned long long acc = 0;
    for(unsigned int i=0; i<op_type_num; ++i){
        acc += op_type_count[i];
    }
    return acc;
}

// 実効情報を表示したうえで異常終了
void exit_with_output(std::string msg){
    std::cout << head_error << msg << std::endl;
    std::cout << head << "abnormal end" << std::endl;
    std::quick_exit(EXIT_FAILURE);
}


/* class Instruction */
inline Instruction::Instruction(){
    this->op = nop;
    this->rs1_v = 0;
    this->rs2_v = 0;
    this->pc = 0;
}

/* class Configuration */
inline Configuration::ID_stage::ID_stage(){ // pcの初期値に注意
    this->pc[0] = -8;
    this->pc[1] = -4;
}

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
                exit_with_output("too many requests for WB(int) (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(inst.pc)))) : "") + ")");
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
                exit_with_output("too many requests for WB(int) (at pc " + std::to_string(inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(inst.pc)))) : "") + ")");
            }
            return;
        case Otype::o_nop: return;
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
            reg_int.write_int(this->inst.op.rd, this->inst.pc + 4);
            return;
        // jal (pass through)
        case Otype::o_jal:
            reg_int.write_int(this->inst.op.rd, this->inst.pc + 4);
            return;
        case Otype::o_nop: return;
        default:
            exit_with_output("invalid operation for AL (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

void Configuration::EX_stage::EX_br::exec(){
    switch(this->inst.op.type){
        // branch
        case Otype::o_beq:
            if(this->inst.rs1_v.i == this->inst.rs2_v.i){
                this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            }
            ++op_type_count[Otype::o_beq];
            return;
        case Otype::o_blt:
            if(this->inst.rs1_v.i < this->inst.rs2_v.i){
                this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            }
            ++op_type_count[Otype::o_blt];
            return;
        // branch_fp
        case Otype::o_fbeq:
            if(this->inst.rs1_v.f == this->inst.rs2_v.f){
                this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            }
            ++op_type_count[Otype::o_fbeq];
            return;
        case Otype::o_fblt:
            if(this->inst.rs1_v.f < this->inst.rs2_v.f){
                this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
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
            this->branch_addr = this->inst.pc + this->inst.op.imm * 4;
            ++op_type_count[Otype::o_jal];
            return;
        case Otype::o_nop: return;
        default:
            exit_with_output("invalid operation for BR (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
    }
}

void Configuration::EX_stage::EX_ma::exec(){
    switch(this->inst.op.type){
        case Otype::o_sw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                write_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4, this->inst.rs2_v);
            }else{
                exit_with_output("address of store operation should be multiple of 4 [sw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[Otype::o_sw];
            return;
        case Otype::o_si:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                op_list[(this->inst.rs1_v.i + this->inst.op.imm) / 4] = Operation(this->inst.rs2_v.i);
            }else{
                exit_with_output("address of store operation should be multiple of 4 [si] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[Otype::o_si];
            return;
        case Otype::o_std:
            send_buffer.push(inst.rs2_v);
            ++op_type_count[Otype::o_std];
            return;
        case Otype::o_fsw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                write_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4, this->inst.rs2_v);
            }else{
                exit_with_output("address of store operation should be multiple of 4 [fsw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[Otype::o_fsw];
            return;
        case Otype::o_lw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                reg_int.write_32(this->inst.op.rd, read_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4));
            }else{
                exit_with_output("address of load operation should be multiple of 4 [lw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
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
                exit_with_output("receive buffer is empty [lrd] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[Otype::o_lrd];
            return;
        case Otype::o_ltf:
            reg_int.write_int(this->inst.op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
            ++op_type_count[Otype::o_ltf];
            return;
        case Otype::o_flw:
            if((this->inst.rs1_v.i + this->inst.op.imm) % 4 == 0){
                reg_fp.write_32(this->inst.op.rd, read_memory((this->inst.rs1_v.i + this->inst.op.imm) / 4));
            }else{
                exit_with_output("address of load operation should be multiple of 4 [flw] (at pc " + std::to_string(this->inst.pc) + (is_debug ? (", line " + std::to_string(id_to_line.left.at(id_of_pc(this->inst.pc)))) : "") + ")");
            }
            ++op_type_count[Otype::o_flw];
            return;
        case Otype::o_nop: return;
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
        case Otype::o_fdiv:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, this->inst.rs1_v.f / this->inst.rs2_v.f);
            }else{
                reg_fp.write_32(this->inst.op.rd, fdiv(this->inst.rs1_v, this->inst.rs2_v));
            }
            ++op_type_count[Otype::o_fdiv];
            return;
        case Otype::o_fsqrt:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, std::sqrt(this->inst.rs1_v.f));
            }else{
                reg_fp.write_32(this->inst.op.rd, fsqrt(this->inst.rs1_v));
            }
            ++op_type_count[Otype::o_fsqrt];
            return;
        case Otype::o_fcvtif:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, static_cast<float>(this->inst.rs1_v.i));
            }else{
                reg_fp.write_32(this->inst.op.rd, itof(this->inst.rs1_v));
            }
            ++op_type_count[Otype::o_fcvtif];
            return;
        case Otype::o_fcvtfi:
            if(is_ieee){
                reg_fp.write_float(this->inst.op.rd, static_cast<int>(std::nearbyint(this->inst.rs1_v.f)));
            }else{
                reg_fp.write_32(this->inst.op.rd, ftoi(this->inst.rs1_v));
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
        case Otype::o_fadd:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f + inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fadd(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[Otype::o_fadd];
            return;
        case Otype::o_fsub:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f - inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fsub(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[Otype::o_fsub];
            return;
        case Otype::o_fmul:
            if(is_ieee){
                reg_fp.write_float(inst.op.rd, inst.rs1_v.f * inst.rs2_v.f);
            }else{
                reg_fp.write_32(inst.op.rd, fmul(inst.rs1_v, inst.rs2_v));
            }
            ++op_type_count[Otype::o_fmul];
            return;
        case Otype::o_nop: return;
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
