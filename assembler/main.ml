open Syntax
open Util
open Parser

let current_id = ref 0 (* 現在見ている命令が何番目かを保持(注意: 行番号とは異なる) *)
let label_bp_list = ref [] (* ラベルorブレークポイント、および対応するidを保持 *)
let label_to_id = ref [] (* ラベルとidの対応関係を保持 *)

(* コマンドライン引数処理用の変数 *)
let filename = ref ""
let is_debug = ref false
let speclist = [
	("-f", Arg.Set_string filename, "Name of the input file");
	("-d", Arg.Set is_debug, "Select debug mode")
]
let usage_msg = "" (* todo *)


(* 機械語への翻訳結果の型 *)
type translation_result =
	| Code of int * string * int * (string option) * (string option) (* 通常の結果: id, 機械語コード, 行番号, (ある場合は)ラベル, (ある場合は)ブレークポイント *)
	| Code_list of string * (int * string * int * (string option) * (string option)) list (* ラベル, そのラベルで翻訳された機械語コードのリスト *)
	| Fail of string * (int * operation * int * (string option) * (string option)) (* ラベルが見つからなかった場合: 未解決のラベル名, (id, 命令, 行番号, (ある場合は)ラベル, (ある場合は)ブレークポイント) *)

(* アセンブル結果の型 *)
type assembling_result = (int * string * (string option)) list (* idとコードと(ある場合は)ラベルのリスト *)

(* 
	アセンブリ言語のコードを翻訳して機械語コードを返す
	code: 処理対象のコード
	untranslated: ラベルが解決されておらず未処理のコードのリスト
	op_id: id
	label_option: (ある場合は)ラベル
*)
exception Translate_error of string
let rec translate_code code untranslated op_id label_option =
	match code with
	| Label label ->
		if List.mem_assoc label !label_bp_list then	
			raise (Translate_error ("label/breakpoint name '" ^ label ^ "' is used more than once")) (* ラベルが重複する場合エラー *)
		else
			(current_id := !current_id - 1; (* ラベルはカウントしないので、idを増やした分を戻す *)
			label_bp_list := (label, op_id) :: !label_bp_list;
			label_to_id := (label, op_id) :: !label_to_id;
			let rec solve_untranslated untranslated = (* 新しいラベルに対応付けられていたuntranslatedを翻訳する *)
				match untranslated with
				| [] -> Code_list (label, [])
				| (op_id', op, line_no, label_option', bp_option') :: rest ->
					let res = translate_code (Operation (op, line_no, bp_option')) [] op_id' label_option' in (* 解決するはずなのでuntranslatedは空でもよい *)
						match (res, solve_untranslated rest) with
						| (Code (id, c, lno, l_o, b_o), Code_list (label', list)) ->
							(((match b_o with
							| Some bp ->
								(try
									if List.assoc bp !label_bp_list == op_id' then () else
										raise (Translate_error ("label/breakpoint name '" ^ bp ^ "' is used more than once")) (* ブレークポイントが重複する場合エラー(ただし同じidに対応するラベルは除く) *)
								with Not_found ->
									label_bp_list := (bp, op_id') :: !label_bp_list) (* 翻訳が成功してからブレークポイントを追加 *)
							| None -> ());
							Code_list (label', (id, c, lno, l_o, b_o) :: list))) (* label'は再帰的に外側のスコープのlabelを渡している *)
						| _ -> raise (Translate_error "upexpected error")
			in solve_untranslated (assoc_all untranslated label))
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
				let opcode = binary_of_int 0 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Sub (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int 0 4 in
				let funct = binary_of_int 1 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Sll (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int 0 4 in
				let funct = binary_of_int 2 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Srl (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int 0 4 in
				let funct = binary_of_int 3 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Sra (rs1, rs2, rd) ->
			if (is_int rs1) && (is_int rs2) && (is_int rd) then
				let opcode = binary_of_int 0 4 in
				let funct = binary_of_int 4 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* op_fp *)
		| Fadd (rs1, rs2, rd) ->
			if (is_float rs1) && (is_float rs2) && (is_float rd) then
				let opcode = binary_of_int 1 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fsub (rs1, rs2, rd) ->
			if (is_float rs1) && (is_float rs2) && (is_float rd) then
				let opcode = binary_of_int 1 4 in
				let funct = binary_of_int 1 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fmul (rs1, rs2, rd) ->
			if (is_float rs1) && (is_float rs2) && (is_float rd) then
				let opcode = binary_of_int 1 4 in
				let funct = binary_of_int 2 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fdiv (rs1, rs2, rd) ->
			if (is_float rs1) && (is_float rs2) && (is_float rd) then
				let opcode = binary_of_int 1 4 in
				let funct = binary_of_int 3 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; rs2; rd; margin] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* branch *)
		| Beq (rs1, rs2, label) ->
			if (is_int rs1) && (is_int rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int 2 4 in
					let funct = binary_of_int 0 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm = binary_of_int_signed (label_id - op_id) 15 in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, label_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, label_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Blt (rs1, rs2, label) ->
			if (is_int rs1) && (is_int rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int 2 4 in
					let funct = binary_of_int 1 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm = binary_of_int_signed (label_id - op_id) 15 in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, label_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, label_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Ble (rs1, rs2, label) ->
			if (is_int rs1) && (is_int rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int 2 4 in
					let funct = binary_of_int 2 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm = binary_of_int_signed (label_id - op_id) 15 in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, label_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, label_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* branch_fp *)
		| Fbeq (rs1, rs2, label) ->
			if (is_float rs1) && (is_float rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int 3 4 in
					let funct = binary_of_int 0 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm = binary_of_int_signed (label_id - op_id) 15 in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, label_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, label_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fblt (rs1, rs2, label) ->
			if (is_float rs1) && (is_float rs2) then
				try
					let label_id = List.assoc label !label_to_id
					in let opcode = binary_of_int 3 4 in
					let funct = binary_of_int 1 3 in
					let rs1 = binary_of_int (int_of_reg rs1) 5 in
					let rs2 = binary_of_int (int_of_reg rs2) 5 in
					let imm = binary_of_int_signed (label_id - op_id) 15 in
					let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
						Code (op_id, code, line_no, label_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, label_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* store *)
		| Sw (rs1, rs2, offset) ->
			if (is_int rs1) && (is_int rs2) then
				let opcode = binary_of_int 4 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = binary_of_int_signed offset 15 in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* store_fp *)
		| Fsw (rs1, rs2, offset) ->
			if (is_int rs1) && (is_float rs2) then
				let opcode = binary_of_int 5 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rs2 = binary_of_int (int_of_reg rs2) 5 in
				let imm = binary_of_int_signed offset 15 in
				let code = String.concat "" [opcode; funct; rs1; rs2; imm] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* op_imm *)
		| Addi (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int 6 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed imm 15 in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Slli (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int 6 4 in
				let funct = binary_of_int 2 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed imm 15 in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Srli (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int 6 4 in
				let funct = binary_of_int 3 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed imm 15 in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Srai (rs1, rd, imm) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int 6 4 in
				let funct = binary_of_int 4 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed imm 15 in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* load *)
		| Lw (rs1, rd, offset) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int 7 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed offset 15 in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* load_fp *)
		| Flw (rs1, rd, offset) ->
			if (is_int rs1) && (is_float rd) then
				let opcode = binary_of_int 8 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed offset 15 in
				let code = String.concat "" [opcode; funct; rs1; String.sub imm 0 5; rd; String.sub imm 5 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* jalr *)
		| Jalr (rs1, rd, offset) ->
			if (is_int rs1) && (is_int rd) then
				let opcode = binary_of_int 9 4 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let imm = binary_of_int_signed offset 18 in
				let code = String.concat "" [opcode; String.sub imm 0 3; rs1; String.sub imm 3 5; rd; String.sub imm 8 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* jal *)
		| Jal (rd, label) ->
			if is_int rd then
				try
					let label_id = List.assoc label !label_to_id in
					let opcode = binary_of_int 10 4 in
					let rd = binary_of_int (int_of_reg rd) 5 in
					let imm = binary_of_int_signed (label_id - op_id) 23 in
					let code = String.concat "" [opcode; String.sub imm 0 13; rd; String.sub imm 13 10] in
						Code (op_id, code, line_no, label_option, bp_option)
				with
				| Not_found -> Fail (label, (op_id, op, line_no, label_option, bp_option))
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* long_imm *)
		| Lui (rd, imm) ->
			if is_int rd then
				let opcode = binary_of_int 11 4 in
				let funct = binary_of_int 0 3 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				if imm < 0 then raise (Translate_error ("long_imm operations does not accept negative immdediates (at line " ^ (string_of_int line_no) ^ ")")) else
				let imm = binary_of_int_signed imm 21 in (* 20桁ぶん確保するためにわざと符号ビットに余裕を持たせている *)
				let code = String.concat "" [opcode; funct; String.sub imm 1 10; rd; String.sub imm 11 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Auipc (rd, imm) ->
			if is_int rd then
				let opcode = binary_of_int 11 4 in
				let funct = binary_of_int 1 3 in
				let rd = binary_of_int (int_of_reg rd) 5 in
				if imm < 0 then raise (Translate_error ("long_imm operations does not accept negative immdediates (at line " ^ (string_of_int line_no) ^ ")")) else
				let imm = binary_of_int_signed imm 21 in (* 20桁ぶん確保するためにわざと符号ビットに余裕を持たせている *)
				let code = String.concat "" [opcode; funct; String.sub imm 1 10; rd; String.sub imm 11 10] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* itof *)
		| Fmvif (rs1, rd) ->
			if (is_int rs1) && (is_float rd) then
				let opcode = binary_of_int 12 4 in
				let funct = binary_of_int 0 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let margin1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin2 = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; margin1; rd; margin2] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fcvtif (rs1, rd) ->
			if (is_int rs1) && (is_float rd) then
				let opcode = binary_of_int 12 4 in
				let funct = binary_of_int 5 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let margin1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin2 = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; margin1; rd; margin2] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		(* ftoi *)
		| Fmvfi (rs1, rd) ->
			if (is_float rs1) && (is_int rd) then
				let opcode = binary_of_int 13 0 in
				let funct = binary_of_int 1 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let margin1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin2 = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; margin1; rd; margin2] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))
		| Fcvtfi (rs1, rd) ->
			if (is_float rs1) && (is_int rd) then
				let opcode = binary_of_int 13 0 in
				let funct = binary_of_int 6 3 in
				let rs1 = binary_of_int (int_of_reg rs1) 5 in
				let margin1 = "00000" in
				let rd = binary_of_int (int_of_reg rd) 5 in
				let margin2 = "0000000000" in
				let code = String.concat "" [opcode; funct; rs1; margin1; rd; margin2] in
					Code (op_id, code, line_no, label_option, bp_option)
			else
				raise (Translate_error ("wrong int/float register designation at line " ^ (string_of_int line_no)))

(* コードのリストをアセンブルする *)
let assemble codes =
	let rec assemble_inner codes untranslated label_option =
		match codes with
		| [] ->
			(match untranslated with
			| [] -> []
			| (label, (id, op, lno, l_o, b_o)) :: rest -> (* 未解決の命令が残っている場合、存在しないラベルを参照する命令があるということなのでエラー *)
				raise (Translate_error ("label '" ^ label ^ "' is not found")))
		| code :: rest ->
			current_id := !current_id + 1;
			(* print_endline (string_of_int !current_id); *)
			match translate_code code untranslated !current_id label_option with
			| Code (id, c, lno, l_o, b_o) ->
				((match b_o with
				| Some bp ->
					(try
						if List.assoc bp !label_bp_list == id then () else
							raise (Translate_error ("label/breakpoint name '" ^ bp ^ "' is used more than once")) (* ブレークポイントが重複する場合エラー(ただし同じidに対応するラベルは除く) *)
					with Not_found ->
						label_bp_list := (bp, id) :: !label_bp_list) (* 翻訳が成功してからブレークポイントを追加 *)
				| None -> ());
				(id, c, lno, l_o, b_o) :: assemble_inner rest untranslated None)
				| Code_list (label, res) -> res @ assemble_inner rest (assoc_delete untranslated label) (Some label) (* 直後の命令の処理にラベルを渡す *)
			| Fail (label, (n, op, lno, l_o, b_o)) -> assemble_inner rest ((label, (n, op, lno, l_o, b_o)) :: untranslated) None
	in assemble_inner codes [] None


let head = "\x1b[1m[asm]\x1b[0m "
let error = "\x1b[1m\x1b[31mError: \x1b[0m"
let () =
	try
		Arg.parse speclist (fun _ -> ()) usage_msg;
		let codes = Parser.toplevel Lexer.token (Lexing.from_channel (open_in ("./source/" ^ !filename ^ ".s"))) in
		print_endline (head ^ "source file: ./source/" ^ !filename ^ ".s");
		let raw_result = assemble codes in
		let result = List.fast_sort (fun (n1, _, _, _, _) (n2, _, _, _, _) -> compare n1 n2) raw_result in (* idでソート *)
		print_endline (head ^ "Succeeded in assembling " ^ !filename ^ ".s\x1b[0m" ^ (if !is_debug then " (in debug-mode)" else ""));
		let out_channel = open_out ("./out/" ^ !filename ^ (if !is_debug then ".dbg" else "")) in
		let rec output_result result = (* アセンブルの結果をidごとにファイルに出力 *)
			match result with
			| [] -> ()
			| (_, c, lno, l_o, b_o) :: rest -> 
				let line = ref c in
				(if !is_debug then
					(line := !line ^ "@" ^ (string_of_int lno);
					(match l_o with
					| Some label -> line := !line ^ "#" ^ label
					| None -> ());
					(match b_o with
					| Some bp -> line := !line ^ "!" ^ bp
					| None -> ()))
				else ());
				Printf.fprintf out_channel "%s\n" !line;
				output_result rest
		in output_result result;
		print_endline (head ^ "output file: ./out/" ^ !filename ^ (if !is_debug then ".dbg" else ""));
		close_out out_channel
	with
	| Failure s -> print_endline (head ^ error ^ s); exit 1
	| Sys_error s -> print_endline (head ^ error ^ s); exit 1
	| Translate_error s -> print_endline (head ^ error ^ s); exit 1
	| Argument_error -> print_endline (head ^ error ^ "internal error"); exit 1