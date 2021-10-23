#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <regex>
#include <thread>
#include <boost/asio.hpp>

std::vector<int> data_received;

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

void server(){
    std::string cmd;
    while(true){
        std::cout << "# " << std::ends;
        if(!std::getline(std::cin, cmd)) break;
        
        std::smatch match;
        if(std::regex_match(cmd, std::regex("^\\s*\\r?\\n?$"))){ // 空行
            // do nothing
        }else if(std::regex_match(cmd, std::regex("^\\s*(q|(quit))\\s*$"))){ // quit
            break;
        }else if(std::regex_match(cmd, match, std::regex("^\\s*(send)\\s+(\\d+)\\s*$"))){
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
        }else if(std::regex_match(cmd, match, std::regex("^\\s*(info)\\s*$"))){
            std::cout << "data list: ";
            for(auto i : data_received){
                std::cout << i << "; ";
            }
            std::cout << std::endl;
        }else{
            std::cout << "invalid dataand" << std::endl;
        }
    }

    return;
}


int main(){
    std::thread t1(server);
    std::thread t2(receive);
    t1.join();
    t2.detach();
}