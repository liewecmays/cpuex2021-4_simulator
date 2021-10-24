#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <regex>
#include <thread>
#include <boost/asio.hpp>

std::vector<int> data_received; // 受け取ったデータのリスト

std::string head = "\x1b[1m[server]\x1b[0m ";
std::string error = "\033[2D\x1b[34m\x1b[1m\x1b[31mError: \x1b[0m";
std::string data = "\033[2D\x1b[34mdata: \x1b[0m";

namespace asio = boost::asio;
using asio::ip::tcp;

// データの受信
void receive(){
    asio::io_service io_service;
    tcp::acceptor acc(io_service, tcp::endpoint(tcp::v4(), 8001));
    tcp::socket socket(io_service);

    boost::system::error_code e;
    while(true){
        acc.accept(socket);

        asio::streambuf buf;
        asio::read(socket, buf, asio::transfer_all(), e);

        if(e && e != asio::error::eof){
            std::cerr << "receive failed (" << e.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }else{
            std::string res = asio::buffer_cast<const char*>(buf.data());
            std::cout << data << "received " << res << std::endl;
            std::cout << "# " << std::flush;
            data_received.emplace_back(std::stoi(res));
        }

        socket.close();
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
        socket.connect(tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 8000));

        boost::system::error_code error;
        asio::write(socket, asio::buffer(match[2].str()), error);

        if(error){
            std::cout << "send failed: " << error.message() << std::endl;
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


int main(){
    std::thread t1(server);
    std::thread t2(receive);
    t1.join();
    t2.detach();
}