#pragma once
#include <common.hpp>
#include <unit.hpp>
#include <sim.hpp>
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>


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
    // int recv_len;

    while(true){
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_size);
        recv(client_socket, buf, 32, 0);
        std::string data(buf);
        memset(buf, '\0', sizeof(buf)); // バッファをクリア
        for(int i=0; i<4; ++i){ // big endianで受信したものとしてバッファに追加
            receive_buffer.push(bit32_of_data(data.substr(i * 8, 8)));
        }

        // while((recv_len = recv(client_socket, buf, 64, 0)) != 0){
        //     send(client_socket, "0", 1, 0); // 成功したらループバック

        //     // 受信したデータの処理
        //     std::string data(buf);
        //     memset(buf, '\0', sizeof(buf)); // バッファをクリア
        //     // if(is_debug){
        //     //     std::cout << head_data << "received " << data << std::endl;
        //     //     if(!loop_flag){
        //     //         std::cout << "\033[2D# " << std::flush;
        //     //     }
        //     // }

        //     if(is_bootloading && data[0] == 'n'){
        //         filename = "boot-" + data.substr(1);
        //         if(is_debug){
        //             std::cout << head_info << "loading ./code/" << data.substr(1) << std::endl;
        //             std::cout << "\033[2D# " << std::flush;
        //         }
        //     }else if(is_loading_codes && data[0] == 't'){ // 渡されてきたラベル・ブレークポイントを処理
        //         // 命令ロード中の場合
        //         if(is_debug){
        //             std::string text = data.substr(1);
        //             std::smatch match;
        //             if(std::regex_match(text, match, std::regex("^@(-?\\d+)$"))){
        //                 id_to_line_loaded.insert(bimap_value_t2(loading_id, std::stoi(match[1].str())));
        //             }else if(std::regex_match(text, match, std::regex("^@(-?\\d+)#(([a-zA-Z_]\\w*(.\\d+)*))$"))){ // ラベルのみ
        //                 id_to_line_loaded.insert(bimap_value_t2(loading_id, std::stoi(match[1].str())));
        //                 label_to_id_loaded.insert(bimap_value_t(match[2].str(), loading_id));             
        //             }else if(std::regex_match(text, match, std::regex("^@(-?\\d+)!(([a-zA-Z_]\\w*(.\\d+)*))$"))){ // ブレークポイントのみ
        //                 id_to_line_loaded.insert(bimap_value_t2(loading_id, std::stoi(match[1].str())));
        //                 bp_to_id_loaded.insert(bimap_value_t(match[2].str(), loading_id));
        //             }else if(std::regex_match(text, match, std::regex("^@(-?\\d+)#(([a-zA-Z_]\\w*(.\\d+)*))!(([a-zA-Z_]\\w*(.\\d+)*))$"))){ // ラベルとブレークポイントの両方
        //                 id_to_line_loaded.insert(bimap_value_t2(loading_id, std::stoi(match[1].str())));
        //                 label_to_id_loaded.insert(bimap_value_t(match[2].str(), loading_id));
        //                 bp_to_id_loaded.insert(bimap_value_t(match[3].str(), loading_id));
        //             }else{
        //                 std::cerr << head_error << "could not parse the received code" << std::endl;
        //                 std::exit(EXIT_FAILURE);
        //             }

        //             ++loading_id;
        //         }else{
        //             std::cerr << head_error << "invalid data received (maybe: put -d option to ./server)" << std::endl;
        //             std::exit(EXIT_FAILURE);
        //         }
        //     }else{
        //         receive_buffer.push(bit32_of_data(data));
        //     }
        // }
        // close(client_socket);
    }
    return;
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
            while(!send_buffer.empty()){
                if(!is_connected){ // 接続されていない場合
                    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    res = connect(client_socket, (struct sockaddr *) &opponent_addr, sizeof(opponent_addr));
                    if(res == 0){
                        is_connected = true;
                    }else{
                        std::cout << head_error << "connection failed (check whether ./server has been started)" << std::endl;
                    }
                }
                
                data = binary_of_int(send_buffer.pop().value().i);
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
