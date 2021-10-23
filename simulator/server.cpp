#include <iostream>
#include <string>
#include <regex>
#include <thread>
#include <boost/asio.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;

// データの受信
void receive(){
    asio::io_service io_service;
    tcp::acceptor acc(io_service, tcp::endpoint(tcp::v4(), 8001));
    tcp::socket socket(io_service);

    boost::system::error_code error;
    while(true){
        acc.accept(socket);

        asio::streambuf buf;
        asio::read(socket, buf, asio::transfer_all(), error);

        if(error && error != asio::error::eof){
            std::cerr << "receive failed (" << error.message() << ")" << std::endl;
            std::exit(EXIT_FAILURE);
        }else{
            std::string data = asio::buffer_cast<const char*>(buf.data());
            std::cout << "received data: " << std::stoi(data) << std::endl;
        }

        socket.close();
    }
    
    return;
}

void server(){
    std::string cmd;
    while(true){
        std::cout << "# " << std::ends;    
        std::getline(std::cin, cmd);
        
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
        }else{
            std::cout << "invalid command" << std::endl;
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