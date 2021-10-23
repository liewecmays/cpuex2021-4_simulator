#include <iostream>
#include <string>
#include <regex>
#include <boost/asio.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;

int main(){
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

    return 0;
}