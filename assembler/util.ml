open Syntax

(* レジスタ名から番号を取り出す *)
let reg_no r =
	try int_of_string (String.sub r 1 2) with
	| Invalid_argument _ -> int_of_string (String.sub r 1 1)

(* 整数を指定された桁数の二進数に変換 *)
exception Argument_error
let binary_of_int n len =
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
let binary_of_int_signed n l =
	if n >= 0 then
		if n >= (1 lsl (l - 1)) then raise Argument_error
		else binary_of_int n l
	else
		if n < - (1 lsl (l - 1)) then raise Argument_error
		else let comp = (1 lsl l) + n in binary_of_int comp l

(* 連想配列からkeyに対応する値を全て取り出す *)
let assoc_all l key =
	let rec assoc_all_inner l key acc =
		match l with
		| [] -> acc
		| (k, v) :: rest -> if key = k then assoc_all_inner rest key (v :: acc) else assoc_all_inner rest key acc
	in assoc_all_inner l key []
