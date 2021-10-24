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
std::vector<int> data_received; // 受け取ったデータのリスト

// ターミナルへの出力用
std::string head = "\x1b[1m[server]\x1b[0m ";
std::string error = "\033[2D\x1b[34m\x1b[1m\x1b[31mError: \x1b[0m";
std::string info = "\033[2D\x1b[34m\x1b[32mInfo: \x1b[0m";
std::string data = "\033[2D\x1b[34mData: \x1b[0m";


/* プロトタイプ宣言 */
void server(); // コマンド入力をもとにデータを送信
bool exec_command(std::string cmd); // コマンドを読み、実行
void receive(); // データの受信


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
                std::cerr << error << "Invalid command-line argument" << std::endl;
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
        std::cout << "# " << std::ends;
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
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(send)\\s+(\\d+)\\s*$"))){ // send N
        asio::io_service io_service;
        tcp::socket socket(io_service);
        boost::system::error_code e;

        socket.connect(tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port), e);
        if(e){
            std::cout << error << "connection failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        asio::write(socket, asio::buffer(match[2].str()), e);
        if(e){
            std::cout << error << "transmission failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        socket.close();
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(send)\\s+([a-zA-Z_]+)\\s*$"))){ // send filename
        std::string filename = match[2].str();
        std::string input_filename = "./data/" + filename + ".dat";
        std::ifstream input_file(input_filename);
        if(!input_file.is_open()){
            std::cerr << error << "could not open " << input_filename << std::endl;
            std::exit(EXIT_FAILURE);
        }else{
            std::cout << "opened file: ./data/" << filename << ".dat" << std::endl;
        }

        std::string line;
        int line_no = 1;
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
            line_no++;
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(info)\\s*$"))){ // info
        std::cout << "data list: ";
        for(auto i : data_received){
            std::cout << i << "; ";
        }
        std::cout << std::endl;
    }else{
        std::cout << "invalid dataand" << std::endl;
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
            std::cerr << error << "connection failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        asio::streambuf buf;
        asio::read(socket, buf, asio::transfer_all(), e);
        if(e && e != asio::error::eof){
            std::cerr << "receive failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        std::string res = asio::buffer_cast<const char*>(buf.data());
        std::cout << data << "received " << res << std::endl;
        std::cout << "# " << std::flush;
        data_received.emplace_back(std::stoi(res));

        socket.close();
    }

    return;
}
