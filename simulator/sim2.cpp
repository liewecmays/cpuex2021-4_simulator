#include <sim2.hpp>
#include <common.hpp>
#include <util.hpp>
#include <fpu.hpp>
#include <string>
#include <iostream>
#include <fstream>
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

using ::Otype;
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
    op_list.resize(code_id + 6); // segmentation fault防止のために余裕を持たせる

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
                                std::cout << head_info << "halt before breakpoint '" + bp << "' (pc " << sim_state << ", line " << id_to_line.left.at(id_of_pc(sim_state)) << ")" << std::endl;
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
