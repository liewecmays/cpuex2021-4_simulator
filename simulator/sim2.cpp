#include <sim2.hpp>
#include <common.hpp>
#include <unit.hpp>
#include <fpu.hpp>
#include <config.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/bimap/bimap.hpp>
#include <regex>
#include <iomanip>
#include <boost/program_options.hpp>
#include <chrono>
#include <exception>
#include <nameof.hpp>

namespace po = boost::program_options;
using enum Otype;
using enum Stype;

/* グローバル変数 */
// 内部処理関係
Configuration config; // 各時点の状態
std::vector<Operation> op_list; // 命令のリスト(PC順)
Reg reg_int; // 整数レジスタ
Reg reg_fp; // 浮動小数点数レジスタ
Memory_with_cache memory; // メモリ(キャッシュは内部)
Fpu fpu; // FPU
TransmissionQueue receive_buffer; // 外部通信での受信バッファ
TransmissionQueue send_buffer; // 外部通信での受信バッファ
BranchPredictor branch_predictor; // 分岐予測器

unsigned int code_size = 0; // コードサイズ
int mem_size = 100; // メモリサイズ
constexpr unsigned long long max_op_count = 10000000000;


// シミュレーションの制御
int sim_state = sim_state_continue; // シミュレータの状態管理
bool is_debug = false; // デバッグモード
bool is_bin = false; // バイナリファイルモード
bool is_raytracing = false; // レイトレ専用モード
bool is_quick = false; // マルチサイクルの処理をすぐに終わったものと見なすモード
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
std::string head_space = "       ";


int main(int argc, char *argv[]){
    // コマンドライン引数をパース
    po::options_description opt("./sim2 option");
	opt.add_options()
        ("help,h", "show help")
        ("file,f", po::value<std::string>(), "filename")
        ("debug,d", "debug mode")
        ("bin,b", "binary-input mode")
        ("mem,m", po::value<int>(), "memory size")
        ("raytracing,r", "specialized for ray-tracing program")
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

    // レイトレを処理する場合は予めreserve
    if(is_raytracing){
        // 命令用のvectorを確保
        op_list.reserve(12000);

        // メモリはこれくらい
        mem_size = 2500000; // 10MB

        // バッファ先読みを有効に
        is_preloading = true;
        preload_filename = "contest";
    }

    // メモリ領域の確保
    memory = Memory_with_cache(mem_size, index_width, offset_width);

    // 分岐予測器
    branch_predictor = BranchPredictor();

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
    unsigned int code_id = 0;
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
    op_list.resize(code_id + 5); // segmentation fault防止のために余裕を持たせる

    // シミュレーションの起動
    simulate();

    // 実行結果の情報を出力
    // if(is_info_output || is_detailed_debug) output_info();

    // レイトレの場合は画像も出力
    if(is_raytracing && sim_state == sim_state_end){
        if(!send_buffer.empty()){
            std::string output_filename = "./out/output_" + timestamp + ".ppm";
            std::ofstream output_file(output_filename);
            if(!output_file){
                std::cerr << head_error << "could not open " << output_filename << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::stringstream output;
            Bit32 b32;
            while(!send_buffer.empty()){
                output << (unsigned char) send_buffer.pop().i;
            }
            output_file << output.str();
            std::cout << head << "output image written in " << output_filename << std::endl;
        }else{
            std::cout << head_error << "send-buffer is empty" << std::endl;
        }
    }
}


// シミュレーションの本体処理
void simulate(){
    try{
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
    }catch(std::exception& e){
        exit_with_output(e);
    }
}

// デバッグモードのコマンドを認識して実行
bool is_in_step = false; // step実行の途中
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
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(u|(until))\\s+(\\d+)\\s*$"))){ // until N
        unsigned int n = std::stoi(match[3].str());
        if(sim_state != sim_state_end){
            while(op_count() < n){
                if((sim_state = config.advance_clock(false, "")) == sim_state_end){
                    std::cout << head_info << "all operations have been simulated successfully!" << std::endl;
                    break;
                }
            }
            if(sim_state != sim_state_end) std::cout << head_info << "executed " << n << " operations" << std::endl;
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(s|(step))\\s*$"))){ // step
        if(sim_state != sim_state_end){
            if(config.EX.br.inst.op.type == o_jalr || config.EX.br.inst.op.type == o_jal){
                int old_pc = config.EX.br.inst.pc;
                is_in_step = true;
                if(sim_state != sim_state_end){
                    exec_command("do");
                    exec_command("break " + std::to_string(id_to_line.left.at(old_pc + 1)) + " __ret");
                    exec_command("continue __ret");
                    exec_command("delete __ret");
                    std::cout << head_info << "step execution around pc " << old_pc << " (line " << id_to_line.left.at(old_pc) << ") " << op_list[old_pc].to_string() << std::endl;
                }
                is_in_step = false;
            }else{
                exec_command("do");
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
            auto end = std::chrono::system_clock::now();
            std::cout << head_info << "all operations have been simulated successfully!" << std::endl;

            // 実行時間などの情報の表示
            if(is_time_measuring){
                double exec_time = std::chrono::duration<double>(end - start).count();
                std::cout << head << "time elapsed (execution): " << exec_time << std::endl;
                unsigned long long cnt = op_count();
                std::cout << head << "operation count: " << cnt << std::endl;
                double op_per_sec = static_cast<double>(cnt) / exec_time;
                std::cout << head << "operations per second: " << op_per_sec << std::endl;

                std::cout << head << "clock count: " << config.clk << std::endl;
                std::cout << head << "prediction: " << std::endl;
                std::cout << head_space << "- execution time: " << transmission_time + static_cast<double>(config.clk) / static_cast<double>(frequency) << std::endl;
                std::cout << head_space << "- clocks per instruction: " << static_cast<double>(config.clk) / static_cast<double>(cnt) << std::endl;
            }
        }else{
            std::cout << head_info << "no operation is left to be simulated" << std::endl;
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(i|(init))\\s*$"))){ // init
        sim_state = sim_state_continue;
        config = Configuration();
        for(unsigned int i=0; i<op_type_num; ++i) op_type_count[i] = 0;
        reg_int = Reg();
        reg_fp = Reg();
        memory = Memory_with_cache(mem_size, index_width, offset_width);
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
                            std::cout << head_info << "halt before breakpoint '" + bp_to_id.right.at(sim_state) << "' (pc " << sim_state << ", line " << id_to_line.left.at(sim_state) << ")" << std::endl;
                        }else{
                            throw std::runtime_error("invalid response from Configuration::advance_clock");
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
                                if(!is_in_step){
                                    std::cout << head_info << "halt before breakpoint '" + bp << "' (pc " << sim_state << ", line " << id_to_line.left.at(sim_state) << ")" << std::endl;
                                }
                            }else{
                                throw std::runtime_error("invalid response from Configuration::advance_clock");
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
            std::cout << "operations executed: " << op_count() << std::endl;
            std::cout << "clk: " << config.clk << std::endl;

            // IF
            std::cout << "\x1b[1m[IF]\x1b[0m";
            for(unsigned int i=0; i<2; ++i){
                std::cout << (i==0 ? " " : "     ") << "if[" << i << "] : pc=" << (config.IF.fetch_addr + i) << ((is_debug && (config.IF.fetch_addr + i) < code_size) ? (", line=" + std::to_string(id_to_line.left.at(config.IF.fetch_addr + i))) : "") << std::endl;
            }


            // EX
            std::cout << "\x1b[1m[EX]\x1b[0m";
            
            // EX_al
            for(unsigned int i=0; i<2; ++i){
                if(!config.EX.als[i].inst.op.is_nop()){
                    std::cout << (i==0 ? " " : "     ") << "al" << i << "   : " << config.EX.als[i].inst.op.to_string() << " (pc=" << config.EX.als[i].inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.EX.als[i].inst.pc))) : "") << ")" << std::endl;
                }else{
                    std::cout << (i==0 ? " " : "     ") << "al" << i << "   :" << std::endl;
                }
            }

            // EX_br
            if(!config.EX.br.inst.op.is_nop()){
                std::cout << "     br    : " << config.EX.br.inst.op.to_string() << " (pc=" << config.EX.br.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.EX.br.inst.pc))) : "") << ")" << std::endl;
            }else{
                std::cout << "     br    :" << std::endl;
            }

            // EX_ma
            // ma1
            if(!config.EX.ma.ma1.inst.op.is_nop()){
                std::cout << "     ma1   : " << config.EX.ma.ma1.inst.op.to_string() << " (pc=" << config.EX.ma.ma1.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.EX.ma.ma1.inst.pc))) : "") << ")" << std::endl;
            }else{
                std::cout << "     ma1   :" << std::endl;
            }

            // ma2
            if(!config.EX.ma.ma2.inst.op.is_nop()){
                std::cout << "     ma2   : " << config.EX.ma.ma2.inst.op.to_string() << " (pc=" << config.EX.ma.ma2.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.EX.ma.ma2.inst.pc))) : "") << ") " << std::endl;
            }else{
                std::cout << "     ma2   :" << std::endl;
            }

            // ma3
            if(!config.EX.ma.ma3.inst.op.is_nop()){
                std::cout << "     ma3   : " << config.EX.ma.ma3.inst.op.to_string() << " (pc=" << config.EX.ma.ma3.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.EX.ma.ma3.inst.pc))) : "") << ")" << std::endl;
            }else{
                std::cout << "     ma3   :" << std::endl;
            }

            // EX_mfp
            if(!config.EX.mfp.inst.op.is_nop()){
                std::cout << "     mfp   : " << config.EX.mfp.inst.op.to_string() << " (pc=" << config.EX.mfp.inst.pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.EX.mfp.inst.pc))) : "") << ") [state: " << NAMEOF_ENUM(config.EX.mfp.state) << (config.EX.mfp.state == MFP_busy ? (", remain: " + std::to_string(config.EX.mfp.remaining_cycle)) : "") << "]" << std::endl;
            }else{
                std::cout << "     mfp   :" << std::endl;
            }

            // EX_pfp
            for(unsigned int i=0; i<pipelined_fpu_stage_num; ++i){
                if(!config.EX.pfp.inst[i].op.is_nop()){
                    std::cout << "     pfp[" << i << "]: " << config.EX.pfp.inst[i].op.to_string() << " (pc=" << config.EX.pfp.inst[i].pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.EX.pfp.inst[i].pc))) : "") << ")" << std::endl;
                }else{
                    std::cout << "     pfp[" << i << "]:" << std::endl;
                }
            }

            // WB
            std::cout << "\x1b[1m[WB]\x1b[0m";
            for(unsigned int i=0; i<2; ++i){
                if(config.WB.inst_int[i].has_value()){
                    std::cout << (i==0 ? " " : "     ") << "int[" << i << "]: " << config.WB.inst_int[i].value().op.to_string() << " (pc=" << config.WB.inst_int[i].value().pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.WB.inst_int[i].value().pc))) : "") << ")" << std::endl;
                }else{
                    std::cout << (i==0 ? " " : "     ") << "int[" << i << "]:" << std::endl;
                }
            }
            for(unsigned int i=0; i<2; ++i){
                if(config.WB.inst_fp[i].has_value()){
                    std::cout << "     fp[" << i << "] : " << config.WB.inst_fp[i].value().op.to_string() << " (pc=" << config.WB.inst_fp[i].value().pc << (is_debug ? (", line=" + std::to_string(id_to_line.left.at(config.WB.inst_fp[i].value().pc))) : "") << ")" << std::endl;
                }else{
                    std::cout << "     fp[" << i << "] :" << std::endl;
                }
            }
        }
        if(bp_to_id.empty()){
            std::cout << "breakpoints: (no breakpoint found)" << std::endl;
        }else{
            std::cout << "breakpoints:" << std::endl;
            for(auto x : bp_to_id.left) {
                std::cout << "  " << x.first << " (pc " << x.second << ", line " << id_to_line.left.at(x.second) << ")" << std::endl;
            }
        }
    }else if(std::regex_match(cmd, std::regex("^\\s*(p|(print))\\s+reg\\s*$"))){ // print reg
        reg_int.print(true, t_default);
        reg_fp.print(false, t_float);
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))(\\s+-(d|b|h|f|o))?(\\s+(x|f)(\\d+))+\\s*$"))){ // print (option) reg
        unsigned int reg_no;
        Stype st = t_default;
        char option = match[4].str().front();
        switch(option){
            case 'd': st = t_dec; break;
            case 'b': st = t_bin; break;
            case 'h': st = t_hex; break;
            case 'f': st = t_float; break;
            case 'o': st = t_op; break;
            default: break;
        }

        while(std::regex_search(cmd, match, std::regex("(x|f)(\\d+)"))){
            reg_no = std::stoi(match[2].str());
            if(match[1].str() == "x"){ // int
                std::cout << "\x1b[1m%x" << reg_no << "\x1b[0m: " << reg_int.read_32(reg_no).to_string(st) << std::endl;
            }else{ // float
                if(st == t_default) st = t_float; // デフォルトはfloat
                std::cout << "\x1b[1m%f" << reg_no << "\x1b[0m: " << reg_fp.read_32(reg_no).to_string(st) << std::endl;
            }
            cmd = match.suffix();
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))(\\s+(-w))?\\s+(m|mem)\\[(\\d+):(\\d+)\\]\\s*$"))){ // print mem[N:M]
        int start = std::stoi(match[6].str());
        int width = std::stoi(match[7].str());
        memory.print(start, width);
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(p|(print))\\s+(rbuf|sbuf)(\\s+(\\d+))?\\s*$"))){ // print rbuf/sbuf N
        unsigned int size;
        if(match[4].str() == ""){
            size = 10; // default
        }else{
            size = std::stoi(match[4].str());
        }

        if(match[3].str() == "rbuf"){
            if(receive_buffer.empty()){
                std::cout << "receive buffer: (empty)" << std::endl;
            }else{
                std::cout << "receive buffer:\n  ";
                receive_buffer.print(size);
            }
        }else if(match[3].str() == "sbuf"){
            if(send_buffer.empty()){
                std::cout << "send buffer: (empty)" << std::endl;
            }else{
                std::cout << "send buffer:\n  ";
                send_buffer.print(size);
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
                if(label_to_id.right.find(id) == label_to_id.right.end() || is_in_step){ // idにはラベルが付いていない？
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
                            if(!is_in_step){
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
                std::cout << head_info << "breakpoint '" << label << "' is now set (at pc " << label_id << ", line " << id_to_line.left.at(label_id) << ")" << std::endl;
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
            if(!is_in_step){
                std::cout << head_info << "breakpoint '" << bp_id << "' is now deleted" << std::endl;
            }
        }else{
            std::cout << head_error << "breakpoint '" << bp_id << "' has not been set" << std::endl;  
        }
    }else if(std::regex_match(cmd, match, std::regex("^\\s*(out)(\\s+(-p|-b))?(\\s+(-f)\\s+(\\w+))?\\s*$"))){ // out (option)
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
            TransmissionQueue copy = send_buffer;
            if(is_ppm){
                while(!copy.empty()){
                    output << (unsigned char) copy.pop().i;
                }
            }else if(is_bin){
                unsigned int i;
                while(!copy.empty()){
                    i = copy.pop().i;
                    output.write((char*) &i, sizeof(char)); // 8bitだけ書き込む
                }
            }else{
                while(!copy.empty()){
                    output << copy.pop().to_string(t_hex) << std::endl;
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

// 実行命令の総数を返す
unsigned long long op_count(){
    unsigned long long acc = 0;
    for(unsigned int i=0; i<op_type_num; ++i){
        acc += op_type_count[i];
    }
    return acc;
}

// 実行情報を表示したうえで異常終了
void exit_with_output(std::exception& e){
    std::cout << head_error << e.what() << std::endl;
    std::cout << head << "abnormal end" << std::endl;
    std::quick_exit(EXIT_FAILURE);
}
