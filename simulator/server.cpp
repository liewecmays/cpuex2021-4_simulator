#include <server.hpp>
#include <params.hpp>
#include <common.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <regex>
#include <unistd.h>
#include <iomanip>


/* グローバル変数 */
std::vector<Bit32> data_received; // 受け取ったデータのリスト
std::string head = "\x1b[1m[server]\x1b[0m "; // ターミナルへの出力用
// bool is_debug = false;

// 通信関連
int port = 20214; // 通信に使うポート番号
struct sockaddr_in opponent_addr; // 通信相手(./sim)の情報
// int client_socket; // 送信用のソケット
// bool is_connected = false; // 通信が維持されているかどうかのフラグ

// ブートローダ関連 -> 廃止
// bool bootloading_start_flag = false; // ブートローダ用通信開始のフラグ
// bool bootloading_end_flag = false; // ブートローダ用通信終了のフラグ


int main(int argc, char *argv[]){
    // コマンドライン引数をパース
    int option;
    std::string filename;
    while ((option = getopt(argc, argv, "p:")) != -1){
        switch(option){
            // case 'd':
            //     is_debug = true;
            //     break;
            case 'p':
                port = std::stoi(std::string(optarg));
                break;
            default:
                std::cerr << head_error << "Invalid command-line argument" << std::endl;
                std::exit(EXIT_FAILURE);
        }
    }

    // コマンドの受け付けとデータ受信処理を別々のスレッドで起動
    std::thread t1(server);
    std::thread t2(receive);
    t1.join();
    t2.detach();
}


// コマンド入力をもとにデータを送信
void server(){
    std::string cmd;
    while(true){
        std::cout << "\033[2D# " << std::flush;
        if(!std::getline(std::cin, cmd)) break;
        if(exec_command(cmd)) break;
    }

    return;
}

// コマンドを読み、実行
bool exec_command(std::string cmd){
    bool res = false;
    std::smatch match;

    if(std::regex_match(cmd, std::regex("^\\s*\\r?\\n?$"))){ // 空行
        // do nothing
    }else if(std::regex_match(cmd, std::regex("^\\s*(q|(quit))\\s*$"))){ // quit
        res = true;
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(send)\\s+(-f|-b)\\s+([a-zA-Z_]+)\\s*$"))){ // send filename
        bool is_bin = match[2].str() == "-b";
        std::string filename = match[3].str();
        std::string input_filename = "./data/" + filename + (is_bin ? ".bin" : ".txt");
        std::ifstream input_file;
        if(is_bin){
            input_file.open(input_filename, std::ios::in | std::ios::binary);
        }else{
            input_file.open(input_filename);
        }
        if(!input_file){
            std::cerr << head_error << "could not open " << input_filename << std::endl;
            std::exit(EXIT_FAILURE);
        }else{
            std::cout << head_info << "opened file: " << input_filename << std::endl;
        }

        if(is_bin){ // バイナリファイルの場合の処理
            int recv_count = 3;
            unsigned char c;
            int acc = 0;
            while(!input_file.eof()){
                input_file.read((char*) &c, sizeof(char)); // 8bit取り出す
                acc += static_cast<int>(c) << (recv_count * 8);
                --recv_count;
                if(recv_count == -1){
                    exec_command("send " + std::to_string(acc)); // todo: なぜか-1が末尾に追加されて送信されている
                    acc = 0;
                    recv_count = 3;
                }
            }
        }else{ // テキストファイルの場合の処理
            std::string line;
            while(std::getline(input_file, line)){
                if(std::regex_match(line, std::regex("^\\s*\\r?\\n?$"))){
                    continue;
                }else{
                    std::stringstream ss{line};
                    std::string buf;
                    while(std::getline(ss, buf, ' ')){
                        if(std::regex_match(buf, std::regex("(\\s|\\t)*\\r?\\n?"))){
                            continue;
                        }else{
                            exec_command("send " + buf);
                        }
                    }
                }
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(send)\\s+(.+)\\s*$"))){ // send N
        std::string input = match[2].str();
        std::string data;
        if(std::regex_match(input, std::regex("(-)?\\d+"))){
            data = binary_of_int(std::stoi(input));
        }else if(std::regex_match(input, std::regex("0f.+"))){
            data = binary_of_float(std::stof(input.substr(2)));
        }else if(std::regex_match(input, std::regex("0b(0|1)+"))){
            data = data_of_binary(input.substr(2));
        // }else if(std::regex_match(input, std::regex("0t.+"))){
        //     data = "t" + input.substr(2);
        // }else if(std::regex_match(input, std::regex("0n.+"))){
        //     data = "n" + input.substr(2);
        }else{
            std::cout << head_error << "invalid argument for 'send'" << std::endl;
            return false;
        }

        // データ送信の準備
        struct in_addr host_addr;
        inet_aton("127.0.0.1", &host_addr);
        opponent_addr.sin_family = AF_INET;
        opponent_addr.sin_port = htons(port);
        opponent_addr.sin_addr = host_addr;

        int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(connect(client_socket, (struct sockaddr *) &opponent_addr, sizeof(opponent_addr)) != 0){
            std::cout << head_error << "connection failed (check whether ./sim has been started)" << std::endl;
        }
        
        send(client_socket, data.c_str(), data.size(), 0);
    // }else if(std::regex_match(cmd, match, std::regex("^\\s*(boot)\\s+([a-zA-Z_]+)\\s*$"))){ // boot filename
    //     std::string filename = match[2].str();
    //     std::string input_filename;
    //     filename += is_debug ? ".dbg" : "";
    //     input_filename = "./code/" + filename;
    //     std::ifstream input_file(input_filename);
    //     if(!input_file.is_open()){
    //         std::cerr << head_error << "could not open " << input_filename << std::endl;
    //         std::exit(EXIT_FAILURE);
    //     }else{
    //         std::cout << head_info << "opened file: " << input_filename << std::endl;
    //     }
        
    //     std::cout << head_info << "waiting for start signal (0x99) ..." << std::endl;
    //     while(true){
    //         std::cout << std::ends;
    //         if(bootloading_start_flag) break;
    //     }
    //     std::cout << head_info << "received start signal (0x99)" << std::endl;

    //     exec_command("send 0n" + filename);

    //     int line_count = 0;
    //     std::string line;
    //     while(std::getline(input_file, line)){
    //         line_count++;
    //     }
    //     std::string line_count_b = binary_of_int(line_count*4);
    //     for(int i=0; i<4; i++){
    //         exec_command("send 0b" + line_count_b.substr(i*8, 8));
    //     }

    //     input_file.clear();
    //     input_file.seekg(0, std::ios::beg);
    //     while(std::getline(input_file, line)){
    //         if(is_debug){
    //             exec_command("send 0t" + line.substr(32));
    //         }
    //         for(int i=0; i<4; i++){
    //             exec_command("send 0b" + line.substr(i*8, 8));
    //         }
    //     }

    //     std::cout << head_info << "waiting for end signal (0xaa) ..." << std::endl;
    //     while(true){
    //         if(bootloading_end_flag) break;
    //     }
    //     std::cout << head_info << "received end signal (0xaa)" << std::endl;
    //     std::cout << head_info << "bootloading end" << std::endl;
    //     bootloading_start_flag = false;
    //     bootloading_end_flag = false;
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(out)(\\s+(-p|-b))?(\\s+(-f)\\s+(\\w+))?\\s*$"))){ // out
        if(!data_received.empty()){
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
            time_t t = time(nullptr);
            tm* time = localtime(&t);
            std::stringstream timestamp;
            timestamp << "20" << time -> tm_year - 100;
            timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_mon + 1;
            timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_mday;
            timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_hour;
            timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_min;
            timestamp << std::setw(2) << std::setfill('0') <<  time -> tm_sec;
            std::string output_filename = "./out/" + filename + "_" + timestamp.str() + ext;
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

            // 出力
            std::stringstream output;
            if(is_ppm){
                for(auto b32 : data_received){
                    output << (unsigned char) b32.i;
                }
            }else if(is_bin){
                for(auto b32 : data_received){
                    output.write((char*) &b32, sizeof(char)); // 8bitだけ書き込む
                }
            }else{
                for(auto b32 : data_received){
                    output << b32.to_string(Stype::t_hex) << std::endl;
                }
            }
            output_file << output.str();
            std::cout << head_info << "data written in " << output_filename << std::endl;
        }else{
            std::cout << head_error << "data buffer is empty" << std::endl;
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(info)\\s*$"))){ // info
        std::cout << "data list: \n  ";
        for(auto b32 : data_received){
            std::cout << b32.to_string(Stype::t_hex) << "; ";
        }
        std::cout << std::endl;
    }else{
        std::cout << head_error << "invalid command" << std::endl;
    }

    return res;
}

// データの受信
void receive(){
    // 受信設定
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port+1);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // 受信の準備
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr));
    listen(server_socket, 5);

    // クライアントの情報を保持する変数など
    struct sockaddr_in client_addr;
    int client_socket;
    int client_addr_size = sizeof(client_addr);
    char buf[8];
    int recv_len;

    while(true){
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_size);
        while((recv_len = recv(client_socket, buf, 8, 0)) != 0){
            send(client_socket, "0", 1, 0); // 成功したらループバック

            // 受信したデータの処理
            std::string data(buf);
            // Bit32 res = bit32_of_data(data);
            // std::cout << head_data << "received " << bit32_of_data(data).to_string(Stype::t_hex) << std::endl;
            // std::cout << "\033[2D# " << std::flush;
            // if(res.i == 153) bootloading_start_flag = true; // ブートローダ用通信の開始
            // if(res.i == 170) bootloading_end_flag = true; // ブートローダ用通信の終了
            data_received.emplace_back(bit32_of_data(data));
        }
        close(client_socket);
    }

    return;
}
