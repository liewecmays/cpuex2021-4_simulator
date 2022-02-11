open Syntax
open Parser

(* 
	
	グローバル変数の定義

*)
let current_id = ref 0 (* 現在見ている命令が何番目かを保持(注意: 行番号とは異なる) *)
let label_bp_list = ref [] (* ラベルorブレークポイント、および対応するidを保持 *)
let label_to_id = ref [] (* ラベルとidの対応関係を保持 *)
let new_line = ref (-1) (* 新しく導入された命令を何行目扱いにするか(0以下) *)

(* コマンドライン引数処理用の変数 *)
let filename = ref ""
let is_debug = ref false
let is_bootloading = ref false
let is_skip = ref false
let is_bin = ref false
let speclist = [
	("-f", Arg.Set_string filename, "filename");
	("-d", Arg.Set is_debug, "debug mode");
	("--boot", Arg.Set is_bootloading, "bootloading mode");
	("-s", Arg.Set is_skip, "bootloading-skip mode");
	("-b", Arg.Set is_bin, "binary-output mode");
]
let usage_msg = "" (* todo *)


(* 

	補助関数の定義

*)
(* レジスタ名から番号を取り出す *)
let int_of_reg r =
	match r with
	| Int_reg i -> i
	| Float_reg i -> i

(* 整数レジスタか否かを判定 *)
let is_int r =
	match r with
	| Int_reg _ -> true
	| Float_reg _ -> false

(* 浮動小数レジスタか否かを判定 *)
let is_float r =
	match r with
	| Int_reg _ -> false
	| Float_reg _ -> true

(* idを絶対アドレスに変換 *)
let address_of_id id =
	if (!is_bootloading || !is_skip) then
		4 * (100 + id - 1)
	else
		4 * (id - 1)

(* 即値を指定された桁数の2進数に変換 *)
exception Argument_error
let rec binary_of_imm imm len =
	match imm with
	| Dec i -> binary_of_int_signed i len
	| Hex h -> binary_of_hex h len
	| Neghex h -> binary_of_neghex h len
	| Label l ->
		let id = List.assoc l !label_to_id in
			binary_of_int_signed (address_of_id id) len (* ブートローダによってロードされるのは命令メモリの100ワード目から *)

(* 整数を指定された桁数の2進数に変換 *)
and binary_of_int n len =
	if n >= (1 lsl len) then raise Argument_error else
	if n < 0 then raise Argument_error else
	let rec binary_of_int_inner n pow acc =
		if pow = 0 then acc else
			if n >= pow then
				binary_of_int_inner (n - pow) (pow / 2) ("1" :: acc)
			else
				binary_of_int_inner n (pow / 2) ("0" :: acc)
	in String.concat "" (List.rev (binary_of_int_inner n (1 lsl (len - 1)) []))

(* binary_of_intで負の値に対応したもの(即値は負になりうるため) *)
and binary_of_int_signed n len =
	if n >= 0 then
		if n >= (1 lsl (len - 1)) then raise Argument_error
		else binary_of_int n len
	else
		if n < - (1 lsl (len - 1)) then raise Argument_error
		else let comp = (1 lsl len) + n in binary_of_int comp len

(* 16進数を2進数に変換 *)
and binary_of_hex h len =
	binary_of_int_signed (dec_of_hex h) len

(* マイナスの付いた16進数を2進数に変換 *)
and binary_of_neghex h len =
	binary_of_int_signed (- dec_of_hex h) len

(* 16進数を10進数に変換 *)
and dec_of_hex h =
	let h_digits = Str.split (Str.regexp "") h in
	let rec dec_of_hex_inner digits i acc =
		match digits with
		| [] -> acc
		| d :: rest ->
			let n =
				match d with
				| "0" -> 0
				| "1" -> 1
				| "2" -> 2
				| "3" -> 3
				| "4" -> 4
				| "5" -> 5
				| "6" -> 6
				| "7" -> 7
				| "8" -> 8
				| "9" -> 9
				| "a" | "A" -> 10
				| "b" | "B" -> 11
				| "c" | "C" -> 12
				| "d" | "D" -> 13
				| "e" | "E" -> 14
				| "f" | "F" -> 15
				| _ -> raise Argument_error
			in dec_of_hex_inner rest (i-1) (acc + n * (1 lsl (i*4))) 
	in dec_of_hex_inner h_digits (List.length h_digits - 1) 0

(* (符号なし)2進数を整数に変換 *)
let int_of_binary b =
	let len = String.length b in
	if len < 32 then (* 0～2^32-1まで対応 *)
		let rec int_of_binary_inner b n base acc =
			if n <= 0 then acc else
				match b.[n-1] with
				| '0' -> int_of_binary_inner b (n-1) (base * 2) acc
				| '1' -> int_of_binary_inner b (n-1) (base * 2) (acc + base)
				| _ -> raise Argument_error
		in int_of_binary_inner b len 1 0
	else
		raise Argument_error


(* 連想配列からkeyに対応する値を全て取り出す *)
let assoc_all l key =
	let rec assoc_all_inner l key acc =
		match l with
		| [] -> acc
		| (k, v) :: rest -> if key = k then assoc_all_inner rest key (v :: acc) else assoc_all_inner rest key acc
	in assoc_all_inner l key []

(* 連想配列からkeysに対応しているものを全て除く *)
let assoc_delete l keys =
	let rec assoc_delete_inner l keys acc =
		match l with
		| [] -> acc
		| (k, v) :: rest -> if List.mem k keys then assoc_delete_inner rest keys acc else assoc_delete_inner rest keys ((k, v) :: acc)
	in assoc_delete_inner l keys []


(* 文字列で表現された機械語コードを4つの整数に変換 *)
let split_line line =
	if String.length line = 32 then
		(
			int_of_binary (String.sub line 0 8), (* ビッグエンディアン *)
			int_of_binary (String.sub line 8 8),
			int_of_binary (String.sub line 16 8),
			int_of_binary (String.sub line 24 8)
		)
	else
		raise Argument_error


(*

	翻訳用の関数の定義

*)
(* 機械語への翻訳結果の型 *)
type translation_result =
	| Code of int * string * int * ((string list) option) * (string option) (* 通常の結果: id, 機械語コード, 行番号, (ある場合は)ラベルのリスト, (ある場合は)ブレークポイント *)
	| Codes of (int * string * int * ((string list) option) * (string option)) list (* 複数命令に展開される場合 *)
	| Code_list_inner of (int * string * int * ((string list) option) * (string option)) list (* 翻訳された機械語コードのリスト(未解決の命令を処理するときに使う内部的なコンストラクタ) *)
	| Code_list of (string list) * (int * string * int * ((string list) option) * (string option)) list (* ラベルのリスト, そのラベルたちで翻訳された機械語コードのリスト *)
	| Fail of string * (int * operation * int * ((string list) option) * (string option)) (* ラベルが見つからなかった場合: 未解決のラベル名, (id, 命令, 行番号, (ある場合は)ラベルのリスト, (ある場合は)ブレークポイント) *)

(* アセンブル結果の型 *)
type assembling_result = (int * string * (string option)) list (* idとコードと(ある場合は)ラベルのリスト *)

(* 
	アセンブリ言語のコードを翻訳して機械語コードを返す
	code: 処理対象のコード
	untranslated: ラベルが解決されておらず未処理のコードのリスト
	op_id: id
	labels_option: (ある場合は)ラベルのリスト
*)
exception Translate_error of string
let rec translate_code code untranslated op_id labels_option =
	match code with
	| Labels labels ->
		current_id := !current_id - 1; (* ラベルはカウントしないので、idを増やした分を戻す *)
		let rec solve_with_label labels' = (* 新しく導入されたラベルそれぞれについてuntranslatedを翻訳 *)
			match labels' with
			| [] -> Code_list (labels, []) (* 外側のスコープのラベル列(今回処理したい全体)を渡す *)
			| label :: rest ->
				let res = (* 先頭のlabelについて解決 *)
					if List.mem_assoc label !label_bp_list then	
						raise (Translate_error ("label/breakpoint name '" ^ label ^ "' is used more than once")) (* ラベルが重複する場合エラー *)
					else
						(label_bp_list := (label, op_id) :: !label_bp_list;
						label_to_id := (label, op_id) :: !label_to_id;
						let rec solve_untranslated untranslated = (* 今見ているラベルに対応付けられていたuntranslatedを翻訳する *)
							match untranslated with
							| [] -> Code_list_inner []
							| (op_id', op, line_no, labels_option', bp_option') :: rest ->
								let res = translate_code (Operation (op, line_no, bp_option')) [] op_id' labels_option' in (* 解決するはずなのでuntranslatedは空でもよい *)
									match (res, solve_untranslated rest) with
									| (Code (id, c, lno, l_o, b_o), Code_list_inner list) ->
										((match b_o with
										| Some bp ->
											(try
												if List.assoc bp !label_bp_list == op_id' then () else
													raise (Translate_error ("label/breakpoint name '" ^ bp ^ "' is used more than once")) (* ブレークポイントが重複する場合エラー(ただし同じidに対応するラベルは除く) *)
											with Not_found ->
												label_bp_list := (bp, op_id') :: !label_bp_list) (* 翻訳が成功してからブレークポイントを追加 *)
										| None -> ());
										Code_list_inner ((id, c, lno, l_o, b_o) :: list))
									| _ -> raise (Translate_error "unexpected error")
						in solve_untranslated (assoc_all untranslated label))
				in
					match (res, solve_with_label rest) with
					| (Code_list_inner op_list1, Code_list (labels', op_list2)) ->
						Code_list (labels', op_list1 @ op_list2)
					| _ -> raise (Translate_error "unexpected error")
		in solve_with_label labels
	| Operation (op, line_no, bp_option) ->
		(match bp_option with
		| Some bp ->
			if not !is_debug then 
				raise (Translate_error "do not designate breakpoints under non-debug-mode") (* デバッグモードでないのにブレークポイントが指定されている場合エラー *)
			else ()
		| None -> ());
		match op with
		(* op *)
		| Add (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int opcode_op 4 in
				let funct = binary_of_int funct_add 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Sub (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int opcode_op 4 in
				let funct = binary_of_int funct_sub 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Sll (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int opcode_op 4 in
				let funct = binary_of_int funct_sll 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Srl (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int opcode_op 4 in
				let funct = binary_of_int funct_srl 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Sra (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int opcode_op 4 in
				let funct = binary_of_int funct_sra 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| And (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int opcode_op 4 in
				let funct = binary_of_int funct_and 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* op_mfp *)
		| Fabs (rs1, rd) ->
			if (is_float rs1) && (is_float rd) then
				let opcode = binary_of_int opcode_op_mfp 4 in
				let funct = binary_of_int funct_fabs 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fneg (rs1, rd) ->
			if (is_float rs1) && (is_float rd) then
				let opcode = binary_of_int opcode_op_mfp 4 in
				let funct = binary_of_int funct_fneg 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fdiv (rs1, rs2, rd) ->
			if (is_float rs1) && (is_float rs2) && (is_float rd) then
				let opcode = binary_of_int opcode_op_mfp 4 in
				let funct = binary_of_int funct_fdiv 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fsqrt (rs1, rd) ->
			if (is_float rs1) && (is_float rd) then
				let opcode = binary_of_int opcode_op_mfp 4 in
				let funct = binary_of_int funct_fsqrt 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fcvtif (rs1, rd) ->
			if (is_float rs1) && (is_float rd) then
				let opcode = binary_of_int opcode_op_mfp 4 in
				let funct = binary_of_int funct_fcvtif 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fcvtfi (rs1, rd) ->
			if (is_float rs1) && (is_float rd) then
				let opcode = binary_of_int opcode_op_mfp 4 in
				let funct = binary_of_int funct_fcvtfi 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fmvff (rs1, rd) ->
			if (is_float rs1) && (is_float rd) then
				let opcode = binary_of_int opcode_op_mfp 4 in
				let funct = binary_of_int funct_fmvff 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* op_pfp *)
		| Fadd (rs1, rs2, rd) ->
			if (is_float rs1) && (is_float rs2) && (is_float rd) then
				let opcode = binary_of_int opcode_op_pfp 4 in
				let funct = binary_of_int funct_fadd 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fsub (rs1, rs2, rd) ->
			if (is_float rs1) && (is_float rs2) && (is_float rd) then
				let opcode = binary_of_int opcode_op_pfp 4 in
				let funct = binary_of_int funct_fsub 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fmul (rs1, rs2, rd) ->
			if (is_float rs1) && (is_float rs2) && (is_float rd) then
				let opcode = binary_of_int opcode_op_pfp 4 in
				let funct = binary_of_int funct_fmul 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* branch *)
		| Beq (rs1, rs2, label) ->
			if (is_int rs1) && (is_int rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int opcode_branch 4 in
					let funct = binary_of_int funct_beq 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm =
						try binary_of_int_signed (label_id - op_id) 15 with
						| Argument_error -> raise (Translate_error ("invalid jump distance at line " ^ (string_of_int line_no))) in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, labels_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, labels_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Blt (rs1, rs2, label) ->
			if (is_int rs1) && (is_int rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int opcode_branch 4 in
					let funct = binary_of_int funct_blt 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm =
						try binary_of_int_signed (label_id - op_id) 15 with
						| Argument_error -> raise (Translate_error ("invalid jump distance at line " ^ (string_of_int line_no))) in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, labels_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, labels_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* branch_fp *)
		| Fbeq (rs1, rs2, label) ->
			if (is_float rs1) && (is_float rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int opcode_branch_fp 4 in
					let funct = binary_of_int funct_fbeq 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm =
						try binary_of_int_signed (label_id - op_id) 15 with
						| Argument_error -> raise (Translate_error ("invalid jump distance at line " ^ (string_of_int line_no))) in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, labels_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, labels_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fblt (rs1, rs2, label) ->
			if (is_float rs1) && (is_float rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int opcode_branch_fp 4 in
					let funct = binary_of_int funct_fblt 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm =
						try binary_of_int_signed (label_id - op_id) 15 with
						| Argument_error -> raise (Translate_error ("invalid jump distance at line " ^ (string_of_int line_no))) in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, labels_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, labels_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* store *)
		| Sw (rs1, rs2, imm) ->
			if (is_int rs1) && (is_int rs2) then
				let opcode = binary_of_int opcode_store 4 in
				let funct = binary_of_int funct_sw 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Si (rs1, rs2, imm) ->
			if (is_int rs1) && (is_int rs2) then
				let opcode = binary_of_int opcode_store 4 in
				let funct = binary_of_int funct_si 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Std rs2 ->
			if is_int rs2 then
				let opcode = binary_of_int opcode_store 4 in
				let funct = binary_of_int funct_std 3 in
				let rs1 = "00000" in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = "000000000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* store_fp *)
		| Fsw (rs1, rs2, imm) ->
			if (is_int rs1) && (is_float rs2) then
				let opcode = binary_of_int opcode_store_fp 4 in
				let funct = binary_of_int funct_fsw 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* op_imm *)
		| Addi (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				try
					let opcode = binary_of_int opcode_op_imm 4 in
					let funct = binary_of_int funct_addi 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rd = binary_of_int (int_of_reg rd) 5 in
					let imm = binary_of_imm imm 15 in (* ここでexceptionの可能性 *)
					let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
						Code (op_id, code, line_no, labels_option, bp_option)
				with
				| Not_found -> (* labelがまだ登場していない *)
					(match imm with
					| Label label -> Fail (label, (op_id, op, line_no, labels_option, bp_option))
					| _ -> raise (Translate_error "upexpected error"))
				| Argument_error -> (* 絶対ジャンプ先がimmに収まっていない -> lui+addiに展開 *)
					match imm with
					| Label l ->
						let address = address_of_id (List.assoc l !label_to_id) in (* Not_foundなら既にはじかれている *)
						if (rd = Int_reg 0) || (rs1 <> Int_reg 0) then
							raise (Translate_error ("upexpected error at line " ^ (string_of_int line_no)))
						else
							let upper20 =  (* 上位20ビット(をluiに入れるために12bitシフトしたもの) *)
								Int32.to_int   
									(Int32.shift_right_logical
										(Int32.logand (Int32.of_int address) (Int32.of_string "0xfffff000")) 
									12) in
							let lower12 = Int32.to_int (Int32.logand (Int32.of_int address) (Int32.of_string "0x00000fff")) in (* 下位12ビット *)
							current_id := !current_id + 1; (* 1命令文追加 *)
							(match
								(translate_code (Operation (Lui (rd, Dec upper20), line_no, bp_option)) [] op_id labels_option,
								translate_code (Operation (Addi (rd, rd, Dec lower12), !new_line, None)) [] (op_id + 1) None)  (* 新規追加のコードは-n行目とする、またブレークポイントやラベルはこの1つ目の命令に持たせる *)
							with
							| (Code (id1, c1, lno1, l_o1, b_o1), Code (id2, c2, lno2, l_o2, b_o2)) ->
								new_line := !new_line - 1;
								Codes [(id1, c1, lno1, l_o1, b_o1); (id2, c2, lno2, l_o2, b_o2)]
							| _ -> raise (Translate_error "upexpected error"))
					| _ -> raise (Translate_error "upexpected error")
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Slli (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int opcode_op_imm 4 in
				let funct = binary_of_int funct_slli 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Srli (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int opcode_op_imm 4 in
				let funct = binary_of_int funct_srli 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Srai (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int opcode_op_imm 4 in
				let funct = binary_of_int funct_srai 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Andi (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int opcode_op_imm 4 in
				let funct = binary_of_int funct_andi 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* load *)
		| Lw (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int opcode_load 4 in
				let funct = binary_of_int funct_lw 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Lre rd ->
			if is_int rd then
				let opcode = binary_of_int opcode_load 4 in
				let funct = binary_of_int funct_lre 3 in
				let rs1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = "000000000000000" in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Lrd rd ->
			if is_int rd then
				let opcode = binary_of_int opcode_load 4 in
				let funct = binary_of_int funct_lrd 3 in
				let rs1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = "000000000000000" in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Ltf rd ->
			if is_int rd then
				let opcode = binary_of_int opcode_load 4 in
				let funct = binary_of_int funct_ltf 3 in
				let rs1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = "000000000000000" in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* load_fp *)
		| Flw (rs1, rd, imm) ->
			if (is_int rs1) && (is_float rd) then
				let opcode = binary_of_int opcode_load_fp 4 in
				let funct = binary_of_int funct_flw 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm =
					try binary_of_imm imm 15 with
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* jalr *)
		| Jalr (rs1, rd) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int opcode_jalr 4 in
				let funct = binary_of_int funct_jalr 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let margin1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin2 = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; margin1; rd; margin2] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* jal *)
		| Jal (rd, label) ->
			if is_int rd then
				try
					let label_id = List.assoc label !label_to_id in
					let opcode = binary_of_int opcode_jal 4 in
					let funct = binary_of_int funct_jal 3 in
					let margin = "00000" in
					let rd = binary_of_int (int_of_reg rd) 5 in
					let imm =
						try binary_of_int_signed (label_id - op_id) 15 with
						| Argument_error -> raise (Translate_error ("invalid jump distance at line " ^ (string_of_int line_no))) in
					let code = String.concat "" [opcode; funct; margin; String.sub imm 0 5; rd; String.sub imm 5 10] in
						Code (op_id, code, line_no, labels_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, labels_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* lui *)
		| Lui (rd, imm) ->
			if is_int rd then
				let opcode = binary_of_int opcode_lui 4 in
				let funct = binary_of_int funct_lui 3 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				(match imm with
				| Dec i ->
					if i < 0 then raise (Translate_error ("long_imm operations does not accept negative immdediates (at line " ^ (string_of_int line_no) ^ ")")) else ()
				| Hex _ -> ()
				| Neghex _ -> raise (Translate_error ("long_imm operations does not accept negative immdediates (at line " ^ (string_of_int line_no) ^ ")"))
				| Label _ -> raise (Translate_error ("long_imm operations does not accept label immdediates (at line " ^ (string_of_int line_no) ^ ")")));
				let imm =
					try binary_of_imm imm 21 with (* 20桁ぶん確保するためにわざと符号ビットに余裕を持たせている *)
					| Argument_error -> raise (Translate_error ("invalid argument at line " ^ (string_of_int line_no))) in
				let code = String.concat "" [opcode; funct; String.sub imm 1 10; rd; String.sub imm 11 10] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* itof *)
		| Fmvif (rs1, rd) ->
			if (is_int rs1) && (is_float rd) then
				let opcode = binary_of_int opcode_itof 4 in
				let funct = binary_of_int funct_fmvif 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let margin1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin2 = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; margin1; rd; margin2] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* ftoi *)
		| Fmvfi (rs1, rd) ->
			if (is_float rs1) && (is_int rd) then
				let opcode = binary_of_int opcode_ftoi 4 in
				let funct = binary_of_int funct_fmvfi 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let margin1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin2 = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; margin1; rd; margin2] in
					Code (op_id, code, line_no, labels_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))

(* コードのリストをアセンブルする *)
let assemble codes =
	let rec assemble_inner codes untranslated labels_option =
		match codes with
		| [] ->
			(match untranslated with
			| [] -> []
			| (label, (id, op, lno, l_o, b_o)) :: rest -> (* 未解決の命令が残っている場合、存在しないラベルを参照する命令があるということなのでエラー *)
				raise (Translate_error ("label '" ^ label ^ "' is not found")))
		| code :: rest ->
			current_id := !current_id + 1;
			(* print_endline (string_of_int !current_id); *)
			match translate_code code untranslated !current_id labels_option with
			| Code (id, c, lno, l_o, b_o) ->
				(match b_o with
				| Some bp ->
					(try
						if List.assoc bp !label_bp_list == id then () else
							raise (Translate_error ("label/breakpoint name '" ^ bp ^ "' is used more than once")) (* ブレークポイントが重複する場合エラー(ただし同じidに対応するラベルは除く) *)
					with Not_found ->
						label_bp_list := (bp, id) :: !label_bp_list) (* 翻訳が成功してからブレークポイントを追加 *)
				| None -> ());
				(id, c, lno, l_o, b_o) :: assemble_inner rest untranslated None
			| Codes res ->
				let rec codes_iter res =
					match res with
					| [] -> assemble_inner rest untranslated None
					| (id, c, lno, l_o, b_o) :: rest' ->
						(match b_o with
						| Some bp ->
							(try
								if List.assoc bp !label_bp_list == id then () else
									raise (Translate_error ("label/breakpoint name '" ^ bp ^ "' is used more than once")) (* ブレークポイントが重複する場合エラー(ただし同じidに対応するラベルは除く) *)
							with Not_found ->
								label_bp_list := (bp, id) :: !label_bp_list) (* 翻訳が成功してからブレークポイントを追加 *)
						| None -> ());
						(id, c, lno, l_o, b_o) :: codes_iter rest'
				in codes_iter res
			| Code_list_inner _ -> raise (Translate_error "unexpected error")
			| Code_list (labels, res) -> res @ assemble_inner rest (assoc_delete untranslated labels) (Some labels) (* ラベル列の直後の命令の処理にそのラベル列を渡す *)
			| Fail (label, (n, op, lno, l_o, b_o)) -> assemble_inner rest ((label, (n, op, lno, l_o, b_o)) :: untranslated) None (* label: 未登場のラベル *)
	in assemble_inner codes [] None


(* 

	メインの制御部

*)
let head = "\x1b[1m[asm]\x1b[0m "
let error = "\x1b[1m\x1b[31mError: \x1b[0m"
let warning = "\x1b[1m\x1b[33mWarning: \x1b[0m"
let () =
	try
		Arg.parse speclist (fun _ -> ()) usage_msg;
		let codes = Parser.toplevel Lexer.token (Lexing.from_channel (open_in ("./source/" ^ !filename ^ ".s"))) in
		print_endline (head ^ "source file: ./source/" ^ !filename ^ ".s");
		let raw_result = assemble codes in
		let result = List.fast_sort (fun (n1, _, _, _, _) (n2, _, _, _, _) -> compare n1 n2) raw_result in (* idでソート *)
		print_endline (head ^ "succeeded in assembling " ^ !filename ^ ".s\x1b[0m" ^ (if !is_debug then " (in debug-mode)" else ""));
		if(!is_bin && !is_debug) then print_endline (warning ^ "debug information is ignored as output is binary file") else ();
		let output_filename = "./out/" ^ !filename ^ (if !is_bin then ".bin" else (if !is_debug then ".dbg" else "")) in
		let out_channel = open_out output_filename in
		let rec output_result result = (* アセンブルの結果をidごとにファイルに出力 *)
			match result with
			| [] -> ()
			| (_, c, lno, l_o, b_o) :: rest -> 
				let line = ref c in
				if !is_bin then (* バイナリ出力モードの場合、行数などの情報をすべて無視 *)
					(let (b1, b2, b3, b4) = split_line !line in
						output_byte out_channel b1;
						output_byte out_channel b2;
						output_byte out_channel b3;
						output_byte out_channel b4)
				else
					((if !is_debug then (* デバッグモードの場合は、行数・ラベル・ブレークポイントの情報を末尾に追加 *)
						(line := !line ^ "@" ^ (string_of_int lno);
						(match l_o with
						| Some labels -> line := !line ^ "#" ^ (List.hd (List.rev labels))
						| None -> ());
						(match b_o with
						| Some bp -> line := !line ^ "!" ^ bp
						| None -> ()))
					else ());
					Printf.fprintf out_channel "%s\n" !line); (* テキスト形式で出力 *)
				output_result rest
		in output_result result;
		print_endline (head ^ "output file: " ^ output_filename);
		close_out out_channel
	with
	| Failure s -> print_endline (head ^ error ^ s); exit 1
	| Sys_error s -> print_endline (head ^ error ^ s); exit 1
	| Translate_error s -> print_endline (head ^ error ^ s); exit 1
	| Argument_error -> print_endline (head ^ error ^ "internal error"); exit 1
	| Not_found -> print_endline (head ^ error ^ "unexpected error (Not_found): there might be an invalid label-immediate"); exit 1 (* addi以外でラベルを即値に取る場合にこのエラーが生じうる *)
