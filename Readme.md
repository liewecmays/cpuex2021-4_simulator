# cpuex2021-4 simulator
## Overview
2021年度CPU実験4班のシミュレータ係による開発物です。



## Description

本レポジトリには以下6つの開発物があります。

- アセンブラ `asm`: 班のISAに従ってアセンブリ言語のコードを機械語コードに変換します。
- 1stシミュレータ `sim`: 機械語コードをもとに命令レベルのシミュレーションを行います。
- 拡張版1stシミュレータ `sim+`: 1stシミュレータにいくつかの機能が追加されています。
- 2ndシミュレータ `sim2`: 機械語コードをもとにクロックレベルのシミュレーションを行います。
- シミュレータ用サーバ `server`: 拡張版1stシミュレータとの通信をします。
- FPU検証ソフトウェア `fpu_test`: C++で記述されたFPUが仕様を満たしていることを検証します。



## Requirement

本レポジトリの開発物を動かすためには、以下のソフトウェア・ライブラリが必要になります。なお、開発および動作確認はWindows10上のwsl1(Ubuntu-18.04)で行いました。
- GNU Make (4.1)
- GNU bash (4.4.20)
- OCaml (4.12.1)
- g++ (11.1.0)
- boost (1.78.0)

なお、サーバを使用した通信を行う場合はファイアウォールの設定が必要になる場合があります。



## Build

ビルドの際は、最上位のディレクトリで以下を実行してください(開発物のソフトウェアが全てビルドされます)。
```bash
$ make all
```
以下のように、一部のソフトウェアのみ選択的にビルドすることもできます(詳細については`Makefile`を参照)。
```bash
$ make 1st  # assembler + 1st simulator
```



## Usage

### using shellscript
アセンブラとシミュレータは、最上位のディレクトリからシェルスクリプトを使って一度に動かすことができます。

```bash
$ ./test.sh [options]
```

内部的には、「指定されたオプションをもとにアセンブラを実行し、その出力ファイルを適宜移動して、指定されたオプションをもとにシミュレータを実行する」という流れになっています。また、オプションをもとに自動的に使用するシミュレータを切り替えています。



#### options

指定可能なコマンドラインオプションについて説明します。

- `-f [filename]`: 読み込むファイル名(拡張子抜き)を指定 **(指定必須)**
  - ファイルは`./source`のディレクトリに入れてください
- `-d`: デバッグモード
- `-b`: バイナリの機械語コードを使うモード
- `-i`: 実行時の情報を出力するモード
  - 注意: デバッグモードと併用する場合、`ctrl + C`などで終了させるとファイルが出力されません。`quit`コマンド(後述)で終了するようにしてください。

- `-m [size]`: シミュレータが内部的に使うメモリのサイズを指定
- `-s`: ブートローディング過程をスキップするモード(命令メモリの100番地以降に読み込まれるものとして動作)
- `--ieee`: シミュレータが内部的に使うFPUをIEEE標準のものに指定
- `--preload`: シミュレータの初期化段階で`contest.bin`を受信バッファに読み込ませる
  - `contest.bin`は`contest.sld`(配布されたもの)をbig endianで変換したもの (`./simulator/data`ディレクトリに格納してください)
- `-r`: レイトレ専用モード(以下を内部的に実行するので、**課題プログラムを動かす際には必ずこれを指定してください**)
  - 初期化段階でメモリのサイズを調整
  - 初期化段階で受信バッファを`contest.bin`で初期化
  - 実行終了時に`.ppm`ファイルを自動で出力 (`./simulator/out`ディレクトリに出力)

以下のオプションを指定すると、内部的に`sim+`が呼び出されます

- `-c`: キャッシュの情報を取得するモード
  - 注意: このモードのもとでは単にキャッシュの情報を内部的に取得するだけなので、ヒット率などの情報を確認したい場合は`-i`オプションをつけたり、`--stat`オプションを併用したりしてください。

- `-g`: 分岐予測のシミュレーションを行うモード
- `--stat`: 詳細な統計情報を取得するモード (`./simulator/info`ディレクトリに出力)
  - 注意: **このモードはデバッグモード(`-d`)のもとで指定しなければ正常に動作しません。**
  - 補足: `-i`オプションを付けない場合でも自動的に実行結果を出力します。

- `--cautious`: メモリの範囲外アクセスを例外として検知し、エラーメッセージを出して異常終了するようにしたモード
- `port [N]`: サーバとの通信の際のポート番号をNに指定する

以下のオプションを指定すると、内部的に`sim2`が呼び出されます。他に指定するオプションとして、上に挙げたもの全てが利用可能なわけではないことに注意してください。

- `-2`: 2ndシミュレータに切り替える



### assembler

アセンブラを動かすためには、`assembler`ディレクトリに移動して以下を実行してください。

```bash
$ ./asm [options]
```

以下に注意してください。

- アセンブル対象のコードは`./assembler/source`ディレクトリに格納してください。
- 指定できるオプションは、`-f [filename]` `-d` `-b` `-s`の4つです(説明はシェルスクリプトで述べた通りです)



### simulators
シミュレータを動かすためには、`simulator`ディレクトリに移動して以下のいずれかを実行してください。
```bash
$ ./sim [options]
$ ./sim+ [options]
$ ./sim2 [options]
```
`-h`オプションを指定するとコマンドラインオプションの説明が見られます(指定可能なオプションは上のシェルスクリプトのところで述べたものとほとんど同様です)。以下に注意してください。

- 実行するコードは`./simulator/code`ディレクトリに格納してください。対応している拡張子は(拡張子ナシ)か`dbg`か`bin`のいずれかです。
- 特に理由がなければ、実行が速い`./sim`を使うことを推奨します。
- シェルスクリプトよりも詳細なオプション指定が可能です。具体的には、
  - `-c [N] [M]`: キャッシュのインデックス幅をN、オフセット幅をMと設定します(指定しなければ本番用のパラメータになります)。
  - `--preload [filename]`: 読み込む`.bin`ファイルの名前を指定できます(指定しなければ`contest.bin`になります)



#### debug mode

デバッグモード(`-d`)のもとでは、以下のようなコマンドを使うことができます。大文字はメタ変数(その文字通りに入力するのではなく何らかの値が入る)で、丸括弧つきのものは省略可能です。

| コマンド         | 略称         | 機能                                                         |
| ---------------- | ------------ | ------------------------------------------------------------ |
| quit             | q            | 終了                                                         |
| do               | d            | 1命令実行し、実行内容を表示 (`sim2`では1クロック進める)      |
| do N             | d N          | N命令実行 (`sim2`ではNクロック進める)                        |
| until N          | u N          | 総命令実行数がNになるまで実行                                |
| step             | s            | 関数呼び出しをスキップして実行 (ステップオーバー実行)<br />**注意**: gdbの`step`とは異なることに注意 |
| run (-t)         | r (-t)       | 終了状態になるまで実行 (`-t`で実行時間などの情報を表示)      |
| init             |              | シミュレーションを初期化<br />(**注意**: プリロードしたバッファの状態を復元できない不具合が発見されています) |
| init run         | ir           | init + run                                                   |
| continue         | c            | 次のブレークポイントの直前まで実行                           |
| continue B       | c B          | ブレークポイントBの直前まで実行                              |
| info             | i            | 実行に関する情報を表示                                       |
| print reg        | p reg        | レジスタの中身を表示                                         |
| print rbuf (N)   | p rbuf (N)   | 受信バッファの中身をN個表示 (指定しなければN=10)             |
| print sbuf (N)   | p sbuf (N)   | 送信バッファの中身をN個表示 (指定しなければN=10)             |
| print (option) R | p (option) R | レジスタを指定して(複数指定可能)オプションに従って中身を表示<br />オプション: `-d`(decimal), `-b`(binary), `-h`(hexadecimal), `-f`(float), `-o`(命令として解釈)<br />例: `p -h x4`, `p -b f2 f3 f4` |
| print mem[M:N]   | p m[M:N]     | メモリのM番地からNワード分表示                               |
| set R N          | s R N        | レジスタRに値Nを(整数として受け取り)代入                     |
| break N B        | b N B        | 入力のN行目にある命令にBというブレークポイントを付ける<br />**注意**: 機械語コードの行数ではなく、`.s`ファイルの行数 |
| break L          | b L          | 入力中でLというラベルがついた命令に(そのラベル名の)ブレークポイントを付ける<br />補足: 先頭一致で名前を検索する機能あり。例えば`read_object.2759`のようなラベル名は`read_obj`で指定でき、もし他に`read_obj`から始まるものがあれば(ブレークポイントは設定せず)その候補を表示する。 |
| delete B         | d B          | Bというブレークポイントを削除                                |
| out (option)     |              | 送信バッファ内のデータをオプションに従って出力(指定しなければファイル名はoutput、拡張子は`.txt`)<br />オプション: `-f A` (ファイル名をAとする), `-b` (バイナリファイル), `-p` (ppmファイル) |



### others

#### server

最上位のディレクトリで`./server.sh`を起動するか、`./simulator`ディレクトリで`./server`を実行するかのいずれかにより、サーバが起動します。`./test.sh`で明示的にポート番号を指定するか、`./simulator`ディレクトリで`./sim+`を実行するかのいずれかにより、このサーバと通信できます。

サーバは以下のようなコマンド入力を受け付けます。

| コマンド        | 機能                                                         |
| --------------- | ------------------------------------------------------------ |
| quit            | 終了 (qに省略可能)                                           |
| send N          | Nという値を(big endianで)32文字のテキストとして送信<br />補足: Nが通常の10進数なら整数として解釈, `0f`から始まる場合は浮動小数点数として解釈, `0b`から始まる場合は2進数として解釈 |
| send (option) F | **[非推奨]** `./data`ディレクトリのFという名前のファイルの中身を送信<br />オプション: `-f`(テキストファイル), `-b`(バイナリファイル)<br />補足: シミュレータ側で`--preload`オプションを指定する方が高速かつ動作が安定しているので、そちらを使うことを推奨します。 |
| info            | 受信したデータを表示                                         |
| out             | 受信したデータをファイルに出力 (オプションや使い方はシミュレータと同様) |



#### FPU verifier

`./simulator`ディレクトリで`./fpu_test`を実行すると、`fpu.hpp`内で記述されたFPUが課題の仕様を満たしていることを(近似的に)検査します。指定可能なオプションは以下の通りです。

- `-h`: オプションに関する説明を表示します。
- `-t [type]`: 検査対象のFPU名を指定します。候補はfadd, fsub, fmul, fdiv, fsqrt, itof, ftoiの7つです。複数個指定することもできます。
- `-i [n]`: 乱数で引数を`n`回生成して検査をします。
- `-e`: 対象のFPUに対して擬似的に全数検査を行います(非常に長い時間がかかることに注意)。具体的には以下を実行します。
  - 1引数関数に対しては全数検査
  - 2引数関数に対しては「片方の引数を全数検査 + もう片方を乱数 × 10」を両側に対して実行



## Demo

`./test.sh`を使ったデモの様子を以下に掲載します。`fib.s`を含む簡単なテストコードが`./source`ディレクトリに入っています。また、`minrt.s`については、4班のコンパイラが出力したものを使ってください。


### Fibonacci (1st sim)

フィボナッチ数列を計算し、値を順にメモリに格納します。

```bash
$ ./test.sh -f fib -d
[asm] source file: ./source/fib.s
[asm] succeeded in assembling fib.s (in debug-mode)
[asm] output file: ./out/fib.dbg
[sim] simulation start
[sim] time elapsed (preparation): 1085
# break loop
Info: breakpoint 'loop' is now set (at pc 5, line 11)
# continue
Info: halt before breakpoint 'loop' (pc 5, line 11)
# continue
Info: halt before breakpoint 'loop' (pc 5, line 11)
# continue
Info: halt before breakpoint 'loop' (pc 5, line 11)
# run
Info: all operations have been simulated successfully!
# print mem[0:10]
mem[0]: 0
mem[1]: 1
mem[2]: 1
mem[3]: 2
mem[4]: 3
mem[5]: 5
mem[6]: 8
mem[7]: 13
mem[8]: 21
mem[9]: 34
# info
operations executed: 84
next: pc 12 (line 20) jal x0, 0
breakpoints:
  loop (pc 5, line 11)
```



### Fibonacci (2nd sim)

`fib-for-debug.s`では、いくつかの行にあらかじめブレークポイントをつけてあります。これに対して2ndシミュレータでサイクル単位の挙動を確認したものが以下のデモです。命令発行や分岐予測の様子が分かります。

```bash
$ ./test.sh -2 -f fib-for-debug -d
[asm] source file: ./source/fib-for-debug.s
[asm] succeeded in assembling fib-for-debug.s (in debug-mode)
[asm] output file: ./out/fib-for-debug.dbg
[sim2] simulation start
# continue loop
clk: 4
[IF] if[0] : pc=8, line=14
     if[1] : pc=9, line=15
[ID] id[0] : blt x2, x1, 7 (pc=5, line=11) -> dispatched [prediction: untaken]
     id[1] : lw x3, 0(x1) (pc=6, line=12) -> not dispatched [Intra_control]
[EX] al0   : addi x2, x0, 10 (pc=4, line=8)
     al1   :
     br    :
     ma[0] :
     ma[1] : sw x1, 1(x0) (pc=0, line=6)
     ma[2] : sw x0, 0(x0) (pc=0, line=4)
     mfp   :
     pfp[0]:
     pfp[1]:
     pfp[2]:
[WB] int[0]: addi x1, x0, 0 (pc=3, line=7)
     int[1]:
     fp[0] :
     fp[1] :
Info: halt before breakpoint 'loop' (pc 5, line 11)
# d
clk: 5
[IF] if[0] : pc=9, line=15
     if[1] : pc=10, line=16
[ID] id[0] : lw x3, 0(x1) (pc=6, line=12) -> dispatched
     id[1] : lw x4, 1(x1) (pc=7, line=13) -> not dispatched [Intra_structural_mem]
[EX] al0   :
     al1   :
     br    : blt x2, x1, 7 (pc=5, line=11) -> untaken [hit]
     ma[0] :
     ma[1] :
     ma[2] : sw x1, 1(x0) (pc=0, line=6)
     mfp   :
     pfp[0]:
     pfp[1]:
     pfp[2]:
[WB] int[0]: addi x2, x0, 10 (pc=4, line=8)
     int[1]:
     fp[0] :
     fp[1] :
# d
clk: 6
[IF] if[0] : pc=10, line=16
     if[1] : pc=11, line=17
[ID] id[0] : lw x4, 1(x1) (pc=7, line=13) -> dispatched
     id[1] : add x5, x3, x4 (pc=8, line=14) -> not dispatched [Intra_RAW_rd_to_rs2]
[EX] al0   :
     al1   :
     br    :
     ma[0] : lw x3, 0(x1) (pc=6, line=12)
     ma[1] :
     ma[2] :
     mfp   :
     pfp[0]:
     pfp[1]:
     pfp[2]:
[WB] int[0]:
     int[1]:
     fp[0] :
     fp[1] :
```



### ray-tracing (1st sim)

課題プログラム(レイトレーシング)を動かすデモです(`-r`オプションが必要です)。

```bash
$ ./test.sh -f minrt -r
[asm] source file: ./source/minrt.s
[asm] succeeded in assembling minrt.s
[asm] output file: ./out/minrt
[sim] simulation start
[sim] preloaded data to the receive-buffer from ./data/contest.bin
[sim] time elapsed (preparation): 5689
Info: all operations have been simulated successfully!
[sim] time elapsed (execution): 13.4521
[sim] operation count: 1364609183
[sim] operations per second: 1.01442e+08
[sim] output image written in ./out/output_2022_0224_1408_48.ppm
```



### ray-tracing (1st sim+)

キャッシュや実行時の統計情報を取得するデモです。統計取得モード(`--stat`)はデバッグモード(`-d`)のもとでしか動かないことに注意してください。なお、実行速度は1stシミュレータに比べて1/2～1/5程度になります。

```bash
$ ./test.sh -f minrt -r -c --stat -d
[asm] source file: ./source/minrt.s
[asm] succeeded in assembling minrt.s (in debug-mode)
[asm] output file: ./out/minrt.dbg
[sim+] simulation start
[sim+] preloaded data to the receive-buffer from ./data/contest.bin
[sim+] time elapsed (preparation): 17730
# run -t
Info: all operations have been simulated successfully!
[sim+] time elapsed (execution): 71.7144
[sim+] operation count: 1364609183
[sim+] operations per second: 1.90284e+07
# quit
[sim+] simulation stat: ./info/minrt-dbg_2022_0224_1409_34.md
[sim+] memory info: ./info/minrt_mem_2022_0224_1409_34.csv
[sim+] execution info: ./info/minrt_exec_2022_0224_1409_34.csv
[sim+] output image written in ./out/output_2022_0224_1409_34.ppm
```



### ray-tracing (2nd sim)

サイクル単位でのシミュレーションを行うデモです。実行時間とCPIの予測を表示します。なお、終了判定の仕方の違いにより、総実行命令数のカウントが1stシミュレータのものより1つ分多くなることがあるので注意してください。また、実行速度は1stシミュレータに比べて1/10程度になります。

**注意**: シミュレータの実装に一部不正確な点があり、この予測は(精度は高いものの)正確ではない可能性が高いです。詳細はレポートを参照してください。

```bash
$ ./test.sh -2 -f minrt -r
[asm] source file: ./source/minrt.s
[asm] succeeded in assembling minrt.s
[asm] output file: ./out/minrt
[sim2] simulation start
[sim2] preloaded data to the receive-buffer from ./data/contest.bin
Info: all operations have been simulated successfully!
[sim2] time elapsed (execution): 141.965
[sim2] operation count: 1364609184
[sim2] operations per second: 9.61227e+06
[sim2] clock count: 1781605291
[sim2] prediction:
       - execution time: 14.728
       - clocks per instruction: 1.30558
[sim2] output image written in ./out/output_2022_0224_1411_49.ppm
```

