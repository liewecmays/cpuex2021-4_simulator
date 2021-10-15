open Syntax
open Util

let line_no = ref 0 (* 現在見ている行番号を保持 *)
let label_to_line = ref [] (* ラベルと行の対応関係を保持 *)

(* 機械語への翻訳結果の型 *)
type translation_result =
	| Code of int * string * (string option) (* 通常の結果: 行番号, 機械語コード, (ある場合は)ラベル *)
	| Code_list of string * (int * string * (string option)) list (* ラベル, そのラベルで翻訳された機械語コードのリスト *)
	| Fail of string * (int * operation * (string option)) (* ラベルが見つからなかった場合: 未解決のラベル名, (行番号, 命令, (ある場合は)ラベル) *)

(* アセンブル結果の型 *)
type assembling_result = (int * string * (string option)) list (* 行番号とコードと(ある場合は)ラベルのリスト *)

(* 
	アセンブリ言語のコードを翻訳して機械語コードを返す
	code: 処理対象のコード
	untranslated: ラベルが解決されておらず未処理のコードのリスト
	line_no_arg: 行番号
	label_option: その行にラベルがついている場合、ラベル
*)
exception Translate_error
let rec translate_code code untranslated line_no_arg label_option =
	match code with
	| Label label ->
		line_no := !line_no - 1; (* ラベルの行はカウントしないので、増やした分を戻す *)
		label_to_line := (label, line_no_arg) :: !label_to_line;
		let rec solve_untranslated untranslated = (* 新しいラベルに対応付けられていたuntranslatedを翻訳する *)
			match untranslated with
			| [] -> Code_list (label, [])
			| (line_no_arg', op, label_option') :: rest ->
				let res = translate_code (Operation op) [] line_no_arg' label_option' in (* 解決するはずなのでuntranslatedは空でもよい *)
					match (res, solve_untranslated rest) with
					| (Code (n, c, l), Code_list (label', list)) -> Code_list (label', (n, c, l) :: list) (* label'は再帰的に外側のスコープのlabelを渡している *)
					| _ -> raise Translate_error
		in solve_untranslated (assoc_all untranslated label)
	| Operation op ->
		match op with
		| Add (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 0 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code, label_option)
		| Sub (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 1 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code, label_option)
		| Sll (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 2 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code, label_option)
		| Srl (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 3 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code, label_option)
		| Sra (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 4 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code, label_option)
		| Beq (rs1, rs2, label) ->
			(try
				let label_line = List.assoc label !label_to_line
				in let opcode = binary_of_int 2 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = binary_of_int_signed (label_line - line_no_arg) 15 in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (line_no_arg, code, label_option)
			with Not_found -> Fail (label, (line_no_arg, op, label_option)))
		| Blt (rs1, rs2, label) ->
			(try
				let label_line = List.assoc label !label_to_line
				in let opcode = binary_of_int 2 4 in
				let funct = binary_of_int 1 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = binary_of_int_signed (label_line - line_no_arg) 15 in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (line_no_arg, code, label_option)
			with Not_found -> Fail (label, (line_no_arg, op, label_option)))
		| Ble (rs1, rs2, label) ->
			(try
				let label_line = List.assoc label !label_to_line
				in let opcode = binary_of_int 2 4 in
				let funct = binary_of_int 2 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = binary_of_int_signed (label_line - line_no_arg) 15 in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (line_no_arg, code, label_option)
			with Not_found -> Fail (label, (line_no_arg, op, label_option)))
		| Sw (rs1, rs2, offset) ->
			let opcode = binary_of_int 3 4 in
			let funct = binary_of_int 0 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let imm = binary_of_int_signed offset 15 in
			let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
				Code (line_no_arg, code, label_option)
		| Addi (rs1, rd, imm) ->
			let opcode = binary_of_int 5 4 in
			let funct = binary_of_int 0 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code, label_option)
		| Slli (rs1, rd, imm) ->
			let opcode = binary_of_int 5 4 in
			let funct = binary_of_int 2 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code, label_option)
		| Srli (rs1, rd, imm) ->
			let opcode = binary_of_int 5 4 in
			let funct = binary_of_int 3 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code, label_option)
		| Srai (rs1, rd, imm) ->
			let opcode = binary_of_int 5 4 in
			let funct = binary_of_int 4 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code, label_option)
		| Lw (rs1, rd, offset) ->
			let opcode = binary_of_int 6 4 in
			let funct = binary_of_int 0 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed offset 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code, label_option)
		| Jalr (rs1, rd, offset) ->
			let opcode = binary_of_int 8 4 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed offset 18 in
			let code = String.concat "" [opcode; String.sub imm 0 3; rs1; String.sub imm 3 5; rd; String.sub imm 8 10] in
				Code (line_no_arg, code, label_option)	
		| Jal (rd, label) ->
			(try
				let label_line = List.assoc label !label_to_line in
				let opcode = binary_of_int 9 4 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed (label_line - line_no_arg) 23 in
				let code = String.concat "" [opcode; String.sub imm 0 13; rd; String.sub imm 13 10] in
					Code (line_no_arg, code, label_option)
			with Not_found -> Fail (label, (line_no_arg, op, label_option)))
		| Lui (rd, imm) ->
			let opcode = binary_of_int 10 4 in
			let funct = binary_of_int 0 3 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 20 in
			let code = String.concat "" [opcode; funct; String.sub imm 0 10; rd; String.sub imm 10 10] in
				Code (line_no_arg, code, label_option)
		| Auipc (rd, imm) ->
			let opcode = binary_of_int 10 4 in
			let funct = binary_of_int 1 3 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 20 in
			let code = String.concat "" [opcode; funct; String.sub imm 0 10; rd; String.sub imm 10 10] in
				Code (line_no_arg, code, label_option)

(* コードのリストをアセンブルする *)
let assemble codes =
	let rec assemble_inner codes untranslated label_option =
		match codes with
		| [] -> []
		| code :: rest ->
			line_no := !line_no + 1;
			print_endline (string_of_int !line_no);
			match translate_code code untranslated !line_no label_option with
			| Code (n, c, l) -> (n, c, l) :: assemble_inner rest untranslated None
			| Code_list (label, res) -> res @ assemble_inner rest untranslated (Some label) (* 直後の命令の処理にラベルを渡す *)
			| Fail (label, (n, op, l)) -> assemble_inner rest ((label, (n, op, l)) :: untranslated) None
	in assemble_inner codes [] None


let filename = ref ""
let is_debug = ref false
let speclist = [
	("-f", Arg.Set_string filename, "Name of the input file");
	("-d", Arg.Set is_debug, "Select debug mode")
]
let usage_msg = "" (* todo *)
let () =
	print_endline "===== assembling start =====";
	Arg.parse speclist (fun _ -> ()) usage_msg;
	let codes = Parser.toplevel Lexer.token (Lexing.from_channel (open_in ("./source/" ^ !filename ^ ".s"))) in
	print_endline ("source file: ./source/" ^ !filename ^ ".s");
	let raw_result = assemble codes in
	let result = List.fast_sort (fun (n1, _, _) (n2, _, _) -> compare n1 n2) raw_result in (* 行番号でソート *)
	let out_channel = open_out ("./out/" ^ !filename ^ (if !is_debug then ".dbg" else "")) in
	let rec output_result result =
		match result with
		| [] -> ()
		| (_, c, l) :: rest -> 
			if !is_debug then
				match l with
				| Some label -> (Printf.fprintf out_channel "%s\n" (c ^ "@" ^ label); output_result rest)
				| None -> (Printf.fprintf out_channel "%s\n" c; output_result rest)
			else
				(Printf.fprintf out_channel "%s\n" c; output_result rest)
	in output_result result;
	print_endline ("output file: ./out/" ^ !filename ^ (if !is_debug then ".dbg" else ""));
	close_out out_channel;
	print_endline "===== assembling completed =====";
