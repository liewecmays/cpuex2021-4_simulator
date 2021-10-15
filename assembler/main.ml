open Syntax
open Util

let line_no = ref 0 (* 現在見ている行番号を保持 *)
let label_to_line = ref [] (* ラベルと行の対応関係を保持 *)

(* 機械語への翻訳結果の型 *)
type translation_result =
	| Code of int * string (* 通常の結果: 行番号, 機械語コード *)
	| Code_list of (int * string) list (* 複数のコードを同時に返す場合 *)
	| Fail of string * (int * operation) (* ラベルが見つからなかった場合: ラベル名, (行番号, 命令) *)

(* 
	アセンブリ言語のコードを翻訳して機械語コードを返す
	code: 処理対象のコード
	untranslated: ラベルが解決されておらず未処理のコードのリスト
	line_no_arg: 行番号
*)
exception Translate_error
let rec translate_code code untranslated line_no_arg =
	match code with
	| Label label ->
		line_no := !line_no - 1; (* ラベルの行はカウントしないので、増やした分を戻す *)
		label_to_line := (label, line_no_arg) :: !label_to_line;
		let rec solve_untranslated untranslated = (* 新しいラベルに対応付けられていたuntranslatedを翻訳する *)
			match untranslated with
			| [] -> Code_list []
			| (line_no_arg', op) :: rest ->
				let res = translate_code (Operation op) [] line_no_arg' in (* 解決するはずなのでuntranslatedは空でもよい *)
					match (res, solve_untranslated rest) with
					| (Code (n, c), Code_list res'') -> Code_list ((n, c) :: res'')
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
				Code (line_no_arg, code)
		| Sub (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 1 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code)
		| Sll (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 2 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code)
		| Srl (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 3 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code)
		| Sra (rs1, rs2, rd) ->
			let opcode = binary_of_int 0 4 in
			let funct = binary_of_int 4 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let margin = "0000000000" in
			let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
				Code (line_no_arg, code)
		| Beq (rs1, rs2, label) ->
			(try
				let label_line = List.assoc label !label_to_line
				in let opcode = binary_of_int 2 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = binary_of_int_signed (label_line - line_no_arg) 15 in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (line_no_arg, code)
			with Not_found -> Fail (label, (line_no_arg, op)))
		| Blt (rs1, rs2, label) ->
			(try
				let label_line = List.assoc label !label_to_line
				in let opcode = binary_of_int 2 4 in
				let funct = binary_of_int 1 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = binary_of_int_signed (label_line - line_no_arg) 15 in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (line_no_arg, code)
			with Not_found -> Fail (label, (line_no_arg, op)))
		| Ble (rs1, rs2, label) ->
			(try
				let label_line = List.assoc label !label_to_line
				in let opcode = binary_of_int 2 4 in
				let funct = binary_of_int 2 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = binary_of_int_signed (label_line - line_no_arg) 15 in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (line_no_arg, code)
			with Not_found -> Fail (label, (line_no_arg, op)))
		| Sw (rs1, rs2, offset) ->
			let opcode = binary_of_int 3 4 in
			let funct = binary_of_int 0 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rs2 = binary_of_int (int_of_reg rs2) 5 in
			let imm = binary_of_int_signed offset 15 in
			let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
				Code (line_no_arg, code)
		| Addi (rs1, rd, imm) ->
			let opcode = binary_of_int 5 4 in
			let funct = binary_of_int 0 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code)
		| Slli (rs1, rd, imm) ->
			let opcode = binary_of_int 5 4 in
			let funct = binary_of_int 2 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code)
		| Srli (rs1, rd, imm) ->
			let opcode = binary_of_int 5 4 in
			let funct = binary_of_int 3 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code)
		| Srai (rs1, rd, imm) ->
			let opcode = binary_of_int 5 4 in
			let funct = binary_of_int 4 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code)
		| Lw (rs1, rd, offset) ->
			let opcode = binary_of_int 6 4 in
			let funct = binary_of_int 0 3 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed offset 15 in
			let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
				Code (line_no_arg, code)
		| Jalr (rs1, rd, offset) ->
			let opcode = binary_of_int 8 4 in
			let rs1 = binary_of_int (int_of_reg rs1) 5 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed offset 18 in
			let code = String.concat "" [opcode; String.sub imm 0 3; rs1; String.sub imm 3 5; rd; String.sub imm 8 10] in
				Code (line_no_arg, code)		
		| Jal (rd, label) ->
			(try
				let label_line = List.assoc label !label_to_line in
				let opcode = binary_of_int 9 4 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed (label_line - line_no_arg) 23 in
				let code = String.concat "" [opcode; String.sub imm 0 13; rd; String.sub imm 13 10] in
					Code (line_no_arg, code)
			with Not_found -> Fail (label, (line_no_arg, op)))
		| Lui (rd, imm) ->
			let opcode = binary_of_int 10 4 in
			let funct = binary_of_int 0 3 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 20 in
			let code = String.concat "" [opcode; funct; String.sub imm 0 10; rd; String.sub imm 10 10] in
				Code (line_no_arg, code)
		| Auipc (rd, imm) ->
			let opcode = binary_of_int 10 4 in
			let funct = binary_of_int 1 3 in
			let rd = binary_of_int (int_of_reg rd) 5 in
			let imm = binary_of_int_signed imm 20 in
			let code = String.concat "" [opcode; funct; String.sub imm 0 10; rd; String.sub imm 10 10] in
				Code (line_no_arg, code)

(* コードのリストをアセンブルする *)
let assemble codes =
	let rec assemble_inner codes untranslated =
		match codes with
		| [] -> []
		| code :: rest ->
			line_no := !line_no + 1;
			(* print_endline (string_of_int !line_no); *)
			match translate_code code untranslated !line_no with
			| Code (n, c) -> (n, c) :: assemble_inner rest untranslated
			| Code_list res -> res @ assemble_inner rest untranslated
			| Fail (label, (n, op)) -> assemble_inner rest ((label, (n, op)) :: untranslated)
	in assemble_inner codes []


let filename = ref ""
let speclist = [("-f", Arg.Set_string filename, "Name of the input file")]
let usage_msg = "" (* todo *)
let () =
	print_endline "===== assembling start =====";
	Arg.parse speclist (fun _ -> ()) usage_msg;
	let codes = Parser.toplevel Lexer.token (Lexing.from_channel (open_in ("./source/" ^ !filename ^ ".s"))) in
	print_endline ("source file: ./source/" ^ !filename ^ ".s");
	let raw_result = assemble codes in
	let result = List.fast_sort (fun (n1, _) (n2, _) -> compare n1 n2) raw_result in (* 行番号でソート *)
	let out_channel = open_out ("./out/" ^ !filename) in
	let rec output_result result =
		match result with
		| [] -> ()
		| (_, c) :: rest ->  Printf.fprintf out_channel "%s\n" c; output_result rest
	in output_result result;
	print_endline ("output file: ./out/" ^ !filename);
	close_out out_channel;
	print_endline "===== assembling completed =====";
