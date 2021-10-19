open Syntax

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


(* 即値を指定された桁数の二進数に変換 *)
exception Argument_error
let rec binary_of_imm imm len =
	match imm with
	| Dec i -> binary_of_int_signed i len
	| Hex h -> binary_of_hex h len
	| Neghex h -> binary_of_neghex h len

(* 整数を指定された桁数の二進数に変換 *)
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


(* 連想配列からkeyに対応する値を全て取り出す *)
let assoc_all l key =
	let rec assoc_all_inner l key acc =
		match l with
		| [] -> acc
		| (k, v) :: rest -> if key = k then assoc_all_inner rest key (v :: acc) else assoc_all_inner rest key acc
	in assoc_all_inner l key []

(* 連想配列からkeyに対応しているものを全て除く *)
let assoc_delete l key =
	let rec assoc_delete_inner l key acc =
		match l with
		| [] -> acc
		| (k, v) :: rest -> if key = k then assoc_delete_inner rest key acc else assoc_delete_inner rest key ((k, v) :: acc)
	in assoc_delete_inner l key []