#include "server.hpp"
#include "common.hpp"
#include "util.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <boost/asio.hpp>
#include <regex>
#include <unistd.h>

namespace asio = boost::asio;
using asio::ip::tcp;

/* グローバル変数 */
int port = 8000; // 通信に使うポート番号
std::vector<Bit32> data_received; // 受け取ったデータのリスト
std::string head = "\x1b[1m[server]\x1b[0m "; // ターミナルへの出力用
bool bootloading_start_flag = false; // ブートローダ用通信開始のフラグ
bool bootloading_end_flag = false; // ブートローダ用通信終了のフラグ


int main(int argc, char *argv[]){
    // コマンドライン引数をパース
    int option;
    std::string filename;
    while ((option = getopt(argc, argv, "dp:")) != -1){
        switch(option){
            case 'd':
                // todo: debug mode
                break;
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
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(send)\\s+(.+)\\s*$"))){ // send N
        asio::io_service io_service;
        tcp::socket socket(io_service);
        boost::system::error_code e;

        socket.connect(tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port), e);
        if(e){
            std::cout << head_error << "connection failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        std::string input = match[2].str();
        std::string data;
        if(std::regex_match(input, std::regex("\\d+"))){
            data = data_of_int(std::stoi(input));
        }else if(std::regex_match(input, std::regex("0b(0|1)+"))){
            data = data_of_binary(input.substr(2));
        }else{
            std::cout << head_error << "invalud argument for 'send'" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        asio::write(socket, asio::buffer(data), e);
        if(e){
            std::cout << head_error << "transmission failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }else{
            // std::cout << head_data << "sent " << input << std::endl;
        }

        socket.close();
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(send)\\s+([a-zA-Z_]+)\\s*$"))){ // send filename
        std::string filename = match[2].str();
        std::string input_filename = "./data/" + filename + ".dat";
        std::ifstream input_file(input_filename);
        if(!input_file.is_open()){
            std::cerr << head_error << "could not open " << input_filename << std::endl;
            std::exit(EXIT_FAILURE);
        }else{
            std::cout << "opened file: " << input_filename << std::endl;
        }

        std::string line;
        while(std::getline(input_file, line)){
            if(std::regex_match(line, std::regex("^\\s*\\r?\\n?$"))){
                continue;
            }else{
                std::stringstream ss{line};
                std::string buf;
                while(std::getline(ss, buf, ' ')){
                    exec_command("send " + buf);
                }
            }
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(boot)\\s+([a-zA-Z_]+)\\s*$"))){ // boot filename
        std::string filename = match[2].str();
        std::string input_filename = "./code/" + filename + ".dbg"; // todo: non-debug modeへの対応
        std::ifstream input_file(input_filename);
        if(!input_file.is_open()){
            std::cerr << head_error << "could not open " << input_filename << std::endl;
            std::exit(EXIT_FAILURE);
        }else{
            std::cout << head_info << "opened file: " << input_filename << std::endl;
        }
        
        std::cout << head_info << "waiting for start signal (0x99) ..." << std::endl;
        while(true){
            if(bootloading_start_flag) break;
        }

        int line_count = 0;
        std::string line;
        while(std::getline(input_file, line)){
            line_count++;
        }
        std::string line_count_b = binary_of_int(line_count*4);
        for(int i=0; i<4; i++){
            exec_command("send 0b" + line_count_b.substr(i*8, 8));
        }

        input_file.clear();
        input_file.seekg(0, std::ios::beg);
        while(std::getline(input_file, line)){
            // Operation(line);
            for(int i=0; i<4; i++){
                exec_command("send 0b" + line.substr(i*8, 8));
            }
        }

        std::cout << head_info << "waiting for end signal (0xaa) ..." << std::endl;
        while(true){
            if(bootloading_end_flag) break;
        }
        std::cout << head_info << "bootloading end" << std::endl;
        bootloading_start_flag = false;
        bootloading_end_flag = false;
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(info)\\s*$"))){ // info
        std::cout << "data list: ";
        for(auto b32 : data_received){
            std::cout << b32.to_string() << "; ";
        }
        std::cout << std::endl;
    }else{
        std::cout << head_error << "invalid command" << std::endl;
    }

    return res;
}

// データの受信
void receive(){
    asio::io_service io_service;
    tcp::acceptor acc(io_service, tcp::endpoint(tcp::v4(), port+1));
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
            std::cerr << "receive failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        std::string data = asio::buffer_cast<const char*>(buf.data());
        std::cout << head_data << "received " << data << std::endl;
        std::cout << "# " << std::flush;
        Bit32 res = bit32_of_data(data);
        if(res.to_int() == 153 && res.t == Type::t_int) bootloading_start_flag = true; // ブートローダ用通信の開始
        if(res.to_int() == 170 && res.t == Type::t_int) bootloading_end_flag = true; // ブートローダ用通信の終了
        data_received.emplace_back(res);

        socket.close();
    }

    return;
}
