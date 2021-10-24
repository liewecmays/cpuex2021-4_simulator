#include "op.hpp"
#include "sim.hpp"
#include "util.hpp"
#include <iostream>
#include <string>
#include <regex>
#include <boost/asio.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;

// 機械語命令をパースする (ラベルやブレークポイントがある場合は処理する)
Operation parse_op(std::string code, int code_id, bool is_init){
    Operation op;
    int opcode, funct, rs1, rs2, rd;    
    opcode = std::stoi(code.substr(0, 4), 0, 2);
    funct = std::stoi(code.substr(4, 3), 0, 2);
    rs1 = std::stoi(code.substr(7, 5), 0, 2);
    rs2 = std::stoi(code.substr(12, 5), 0, 2);
    rd = std::stoi(code.substr(17, 5), 0, 2);
    
    op.opcode = opcode;
    switch(opcode){
        case 0: // op
        case 1: // op_fp
            op.funct = funct;
            op.rs1 = rs1;
            op.rs2 = rs2;
            op.rd = rd;
            op.imm = -1;
            break;
        case 2: // branch
        case 3: // branch_fp
        case 4: // store
        case 5: // store_fp
            op.funct = funct;
            op.rs1 = rs1;
            op.rs2 = rs2;
            op.rd = -1;
            op.imm = binary_stoi(code.substr(17, 15));
            break;
        case 6: // op_imm
        case 7: // load
        case 8: // load_fp
            op.funct = funct;
            op.rs1 = rs1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = binary_stoi(code.substr(12, 5) + code.substr(22, 10));
            break;
        case 9: // jalr
            op.funct = -1;
            op.rs1 = rs1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = binary_stoi(code.substr(4, 3) + code.substr(12, 5) + code.substr(22, 10));
            break;
        case 10: // jal
            op.funct = -1;
            op.rs1 = -1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = binary_stoi(code.substr(4, 13) + code.substr(22, 10));
            break;
        case 11: // long_imm
            op.funct = funct;
            op.rs1 = -1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = binary_stoi("0" + code.substr(7, 10) + code.substr(22, 10));
            break;
        case 12: // itof
        case 13: // ftoi
            op.funct = funct;
            op.rs1 = rs1;
            op.rs2 = -1;
            op.rd = rd;
            op.imm = -1;
            break;
        default:
            std::cerr << error << "could not parse the code" << std::endl;
            std::exit(EXIT_FAILURE);
    }

    // ラベル・ブレークポイントの処理
    if(is_init && code.size() > 32){
        if(is_debug){ // デバッグモード
            std::smatch match;
            if(std::regex_match(code, match, std::regex("^\\d{32}@(\\d+)$"))){
                id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
            }else if(std::regex_match(code, match, std::regex("^\\d{32}@(\\d+)#(([a-zA-Z_]\\w*(.\\d+)?))$"))){ // ラベルのみ
                id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                label_to_id.insert(bimap_value_t(match[2].str(), code_id));             
            }else if(std::regex_match(code, match, std::regex("^\\d{32}@(\\d+)!(([a-zA-Z_]\\w*(.\\d+)?))$"))){ // ブレークポイントのみ
                id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                bp_to_id.insert(bimap_value_t(match[2].str(), code_id));
            }else if(std::regex_match(code, match, std::regex("^\\d{32}@(\\d+)#(([a-zA-Z_]\\w*(.\\d+)?))!(([a-zA-Z_]\\w*(.\\d+)?))$"))){ // ラベルとブレークポイントの両方
                id_to_line.insert(bimap_value_t2(code_id, std::stoi(match[1].str())));
                label_to_id.insert(bimap_value_t(match[2].str(), code_id));
                bp_to_id.insert(bimap_value_t(match[3].str(), code_id));
            }else{
                std::cerr << error << "could not parse the code" << std::endl;
                std::exit(EXIT_FAILURE);
            }
        }else{ // デバッグモードでないのにラベルやブレークポイントの情報が入っている場合エラー
            std::cerr << error << "could not parse the code (maybe it is encoded in debug-style)" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    op_total++;
    return op;
}

// 命令を実行し、PCを変化させる
void exec_op(Operation &op){
    if(is_out){
        if(is_debug){
            output << op_count << ": pc " << pc << ", line " << id_to_line.left.at(id_of_pc(pc)) << " (" << string_of_op(op) << ")\n";
        }else{
            output << op_count << ": pc " << pc << " (" << string_of_op(op) << ")\n";
        }
    }

    switch(op.opcode){
        case 0: // op
            switch(op.funct){
                case 0: // add
                    write_reg(op.rd, read_reg(op.rs1) + read_reg(op.rs2));
                    pc += 4;
                    return;
                case 1: // sub
                    write_reg(op.rd, read_reg(op.rs1) - read_reg(op.rs2));
                    pc += 4;
                    return;
                case 2: // sll
                    write_reg(op.rd, read_reg(op.rs1) << read_reg(op.rs2));
                    pc += 4;
                    return;
                case 3: // srl
                    write_reg(op.rd, static_cast<unsigned int>(read_reg(op.rs1)) >> read_reg(op.rs2));
                    pc += 4;
                    return;
                case 4: // sra
                    write_reg(op.rd, read_reg(op.rs1) >> read_reg(op.rs2)); // todo: 処理系依存
                    pc += 4;
                    return;
                case 5: // andi
                    write_reg(op.rd, read_reg(op.rs1) & read_reg(op.rs2));
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 1: // op_fp todo: 仕様に沿っていないので注意
            switch(op.funct){
                case 0: // fadd
                    write_reg_fp(op.rd, read_reg_fp(op.rs1) + read_reg_fp(op.rs2));
                    pc += 4;
                    return;
                case 1: // fsub
                    write_reg_fp(op.rd, read_reg_fp(op.rs1) - read_reg_fp(op.rs2));
                    pc += 4;
                    return;
                case 2: // fmul
                    write_reg_fp(op.rd, read_reg_fp(op.rs1) * read_reg_fp(op.rs2));
                    pc += 4;
                    return;
                case 3: // fdiv
                    write_reg_fp(op.rd, read_reg_fp(op.rs1) / read_reg_fp(op.rs2));
                    pc += 4;
                    return;
                case 4: // fsqrt
                    write_reg_fp(op.rd, std::sqrt(read_reg_fp(op.rs1)));
                    pc += 4;
                    return;
            }
            break;
        case 2: // branch
            switch(op.funct){
                case 0: // beq
                    read_reg(op.rs1) == read_reg(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                case 1: // blt
                    read_reg(op.rs1) < read_reg(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                case 2: // ble
                    read_reg(op.rs1) <= read_reg(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                default: break;
            }
            break;
        case 3: // branch_fp
            switch(op.funct){
                case 0: // fbeq
                    read_reg_fp(op.rs1) == read_reg_fp(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                case 1: // fblt
                    read_reg_fp(op.rs1) < read_reg_fp(op.rs2) ? pc += op.imm * 4 : pc += 4;
                    return;
                default: break;
            }
            break;
        case 4: // store
            switch(op.funct){
                case 0: // sw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        memory[(read_reg(op.rs1) + op.imm) / 4].i = read_reg(op.rs2);
                    }else{
                        std::cerr << error << "address of store operation should be multiple of 4" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 1: // si
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        std::stringstream code;
                        code << std::bitset<32>(read_reg(op.rs2));
                        op_list[(read_reg(op.rs1) + op.imm) / 4] = parse_op(code.str(), 0, false);
                    }else{
                        std::cerr << error << "address of store operation should be multiple of 4" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 2: // std
                    {
                        asio::io_service io_service;
                        tcp::socket socket(io_service);
                        boost::system::error_code e;

                        socket.connect(tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port+1), e);
                        if(e){
                            std::cout << error << "connection failed (" << e.message() << ")" << std::endl;
                            std::exit(EXIT_FAILURE);
                        }
                        
                        asio::write(socket, asio::buffer(std::to_string(read_reg(op.rs2))), e);
                        if(e){
                            std::cout << error << "data transmission failed (" << e.message() << ")" << std::endl;
                            std::exit(EXIT_FAILURE);
                        }

                        if(is_debug){
                            std::cout << data << "sent " << read_reg(op.rs2) << std::endl;
                            if(!loop_flag){
                                std::cout << "# " << std::flush;
                            }
                        }

                        socket.close();
                    }
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 5: // store_fp
            switch(op.funct){
                case 0: // fsw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        memory[(read_reg(op.rs1) + op.imm) / 4].f = read_reg_fp(op.rs2);
                    }else{
                        std::cerr << error << "address of store operation should be multiple of 4" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 2: // fstd
                    {
                        asio::io_service io_service;
                        tcp::socket socket(io_service);
                        boost::system::error_code e;

                        socket.connect(tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 8001));
                        if(e){
                            std::cout << error << "connection failed (" << e.message() << ")" << std::endl;
                            std::exit(EXIT_FAILURE);
                        }
                        
                        Int_float u;
                        u.f = read_reg_fp(op.rs2);
                        asio::write(socket, asio::buffer(std::to_string(u.i)), e);
                        if(e){
                            std::cout << error << "data transmission failed (" << e.message() << ")" << std::endl;
                            std::exit(EXIT_FAILURE);
                        }

                        if(is_debug){
                            std::cout << data << "sent " << u.f << std::endl;
                            if(!loop_flag){
                                std::cout << "# " << std::flush;
                            }
                        }

                        socket.close();
                    }
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 6: // op_imm
            switch(op.funct){
                case 0: // addi
                    write_reg(op.rd, read_reg(op.rs1) + op.imm);
                    pc += 4;
                    return;
                case 2: // slli
                    write_reg(op.rd, read_reg(op.rs1) << op.imm);
                    pc += 4;
                    return;
                case 3: // srli
                    write_reg(op.rd, static_cast<unsigned int>(read_reg(op.rs1)) >> op.imm);
                    pc += 4;
                    return;
                case 4: // srai
                    write_reg(op.rd, read_reg(op.rs1) >> op.imm); // todo: 処理系依存
                    pc += 4;
                    return;
                case 5: // andi
                    write_reg(op.rd, read_reg(op.rs1) & op.imm);
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 7: // load
            switch(op.funct){
                case 0: // lw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        write_reg(op.rd, memory[(read_reg(op.rs1) + op.imm) / 4].i);
                    }else{
                        std::cerr << error << "address of load operation should be multiple of 4" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 1: // lre
                    write_reg(op.rd, receive_buffer.empty() ? 1 : 0);
                    pc += 4;
                    return;
                case 2: // lrd
                    if(!receive_buffer.empty()){
                        write_reg(op.rd, receive_buffer.front());
                        receive_buffer.pop();
                    }else{
                        std::cerr << error << "receive buffer is empty" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                case 3: // ltf
                    write_reg(op.rd, 0); // 暫定的に、常にfull flagが立っていない(=送信バッファの大きさに制限がない)としている
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 8: // load_fp
            switch(op.funct){
                case 0: // flw
                    if((read_reg(op.rs1) + op.imm) % 4 == 0){
                        write_reg_fp(op.rd, memory[(read_reg(op.rs1) + op.imm) / 4].f);
                    }else{
                        std::cerr << error << "address of load operation should be multiple of 4" << std::endl;
                    }
                    pc += 4;
                    return;
                case 2: // lrd
                    if(!receive_buffer.empty()){
                        Int_float u;
                        u.i = receive_buffer.front();
                        write_reg_fp(op.rd, u.f);
                        receive_buffer.pop();
                    }else{
                        std::cerr << error << "receive buffer is empty" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 9: // jalr
            write_reg(op.rd, pc + 4);
            pc = read_reg(op.rs1) + op.imm * 4;
            return;
        case 10: // jal
            write_reg(op.rd, pc + 4);
            pc += op.imm * 4;
            return;
        case 11: // long_imm
            switch(op.funct){
                case 0: // lui
                    write_reg(op.rd, op.imm << 12);
                    pc += 4;
                    return;
                case 1: // auipc
                    write_reg(op.rd, pc + (op.imm << 12));
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 12: // itof
            switch(op.funct){
                Int_float u;
                case 0: // fmv.i.f
                    u.i = read_reg(op.rs1);
                    write_reg_fp(op.rd, u.f);
                    pc += 4;
                    return;
                case 5: // fcvt.i.f
                    write_reg_fp(op.rd, static_cast<float>(read_reg(op.rs1)));
                    pc += 4;
                    return;
                default: break;
            }
            break;
        case 13: // ftoi
            switch(op.funct){
                Int_float u;
                case 0: // fmv.f.i
                    u.f = read_reg_fp(op.rs1);
                    write_reg(op.rd, u.i);
                    pc += 4;
                    return;
                case 6: // fcvt.f.i
                    write_reg(op.rd, static_cast<int>(read_reg_fp(op.rs1)));
                    pc += 4;
                    return;
                default: break;
            }
            break;
        default: break;
    }

    std::cerr << error << "error in executing the code" << std::endl;
    std::exit(EXIT_FAILURE);
}

// 命令を文字列に変換
std::string string_of_op(Operation &op){
    std::string res = "";
    switch(op.opcode){
        case 0: // op
            switch(op.funct){
                case 0: // add
                    res += "add ";
                    break;
                case 1: // sub
                    res += "sub ";
                    break;
                case 2: // sll
                    res += "sll ";
                    break;
                case 3: // srl
                    res += "srl ";
                    break;
                case 4: // sra
                    res += "sra ";
                    break;
                case 5: // and
                    res += "and ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=x" + std::to_string(op.rs2) + ", ");
            res += ("rd=x" + std::to_string(op.rd));
            return res;
        case 1: // op_fp
            switch(op.funct){
                case 0: // fadd
                    res += "fadd ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 1: // fsub
                    res += "fsub ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 2: // fmul
                    res += "fmul ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 3: // fdiv
                    res += "fdiv ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 4: // fsqrt
                    res += "fsqrt ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        case 2: // branch
            switch(op.funct){
                case 0: // beq
                    res += "beq ";
                    break;
                case 1: // blt
                    res += "blt ";
                    break;
                case 2: // ble
                    res += "ble ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rs2=x" + std::to_string(op.rs2) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 3: // branch_fp
            switch(op.funct){
                case 0: // fbeq
                    res += "fbeq ";
                    break;
                case 1: // fblt
                    res += "fblt ";
                    break;
            }
            res += ("rs1=f" + std::to_string(op.rs1) + ", ");
            res += ("rs2=f" + std::to_string(op.rs2) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 4: // store
            switch(op.funct){
                case 0: // sw
                    res += "sw ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=x" + std::to_string(op.rs2) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 1: // si
                    res += "si ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=x" + std::to_string(op.rs2) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 2: // std
                    res += "std ";
                    res += ("rs2=x" + std::to_string(op.rs2));
                    break;
                default: return "";
            }
            
            return res;
        case 5: // store_fp
            switch(op.funct){
                case 0: // fsw
                    res += "fsw ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rs2=f" + std::to_string(op.rs2) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 1: // fstd
                    res += "fstd ";
                    res += ("rs1=f" + std::to_string(op.rs1));
                    break;
                default: return "";
            }
            return res;
        case 6: // op_imm
            switch(op.funct){
                case 0: // addi
                    res += "addi ";
                    break;
                case 2: // slli
                    res += "slli ";
                    break;
                case 3: // srli
                    res += "srli ";
                    break;
                case 4: // srai
                    res += "srai ";
                    break;
                case 5: // andi
                    res += "andi ";
                    break;
                default: return "";
            }
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 7: // load
            switch(op.funct){
                case 0: // lw
                    res += "lw ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rd=x" + std::to_string(op.rd) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 1: // lre
                    res += "lre ";
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                case 2: // lrd
                    res += "lrd ";
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                case 3: // ltf
                    res += "ltf ";
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        case 8: // load_fp
            switch(op.funct){
                case 0: // lw
                    res += "flw ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rd=f" + std::to_string(op.rd) + ", ");
                    res += ("imm=" + std::to_string(op.imm));
                    break;
                case 2: // flrd
                    res += "flrd ";
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        case 9: // jalr
            res = "jalr ";
            res += ("rs1=x" + std::to_string(op.rs1) + ", ");
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 10: // jal
            res = "jal ";
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 11: // long_imm
            switch(op.funct){
                case 0: // lui
                    res += "lui ";
                    break;
                case 1: // auipc
                    res += "auipc ";
                    break;
                default: return "";
            }
            res += ("rd=x" + std::to_string(op.rd) + ", ");
            res += ("imm=" + std::to_string(op.imm));
            return res;
        case 12: // itof
            switch(op.funct){
                case 0: // fmv.i.f
                    res += "fmv.i.f ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                case 5: // fcvt.i.f
                    res += "fcvt.i.f ";
                    res += ("rs1=x" + std::to_string(op.rs1) + ", ");
                    res += ("rd=f" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        case 13: // ftoi
            switch(op.funct){
                case 0: // fmv.f.i
                    res += "fmv.f.i ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                case 6: // fcvt.f.i
                    res += "fcvt.f.i ";
                    res += ("rs1=f" + std::to_string(op.rs1) + ", ");
                    res += ("rd=x" + std::to_string(op.rd));
                    break;
                default: return "";
            }
            return res;
        default: return "";
    }
}
