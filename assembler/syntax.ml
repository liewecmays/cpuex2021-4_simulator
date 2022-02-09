type reg =
	| Int_reg of int
	| Float_reg of int

type imm =
	| Dec of int (* 10進数 *)
	| Hex of string (* 16進数 *)
	| Neghex of string (* マイナスの付いた16進数 *)
	| Label of string (* ラベル *)

type operation =
	(* op *)
	| Add of reg * reg * reg (* rs1, rs2, rd *)
	| Sub of reg * reg * reg (* rs1, rs2, rd *)
	| Sll of reg * reg * reg (* rs1, rs2, rd *)
	| Srl of reg * reg * reg (* rs1, rs2, rd *)
	| Sra of reg * reg * reg (* rs1, rs2, rd *)
	| And of reg * reg * reg (* rs1, rs2, rd *)
	(* op_mfp *)
	| Fabs of reg * reg (* rs1, rd *)
	| Fneg of reg * reg (* rs1, rd *)
	| Fdiv of reg * reg * reg (* rs1, rs2, rd *)
	| Fsqrt of reg * reg (* rs1, rd *)
	| Fcvtif of reg * reg (* rs1, rd *)
	| Fcvtfi of reg * reg (* rs1, rd *)
	| Fmvff of reg * reg (* rs1, rd *)
	(* op_pfp *)
	| Fadd of reg * reg * reg (* rs1, rs2, rd *)
	| Fsub of reg * reg * reg (* rs1, rs2, rd *)
	| Fmul of reg * reg * reg (* rs1, rs2, rd *)
	(* branch *)
	| Beq of reg * reg * string (* rs1, rs2, label *)
	| Blt of reg * reg * string (* rs1, rs2, label *)
	(* branch_fp *)
	| Fbeq of reg * reg * string (* rs1, rs2, label *)
	| Fblt of reg * reg * string (* rs1, rs2, label *)
	(* store *)
	| Sw of reg * reg * imm (* rs1, rs2, offset *)
	| Si of reg * reg * imm (* rs1, rs2, offset *)
	| Std of reg (* rs1 *)
	(* store_fp *)
	| Fsw of reg * reg * imm (* rs1, rs2, offset *)
	(* op_imm *)
	| Addi of reg * reg * imm (* rs1, rd, imm *)
	| Slli of reg * reg * imm (* rs1, rd, imm *)
	| Srli of reg * reg * imm (* rs1, rd, imm *)
	| Srai of reg * reg * imm (* rs1, rd, imm *)
	| Andi of reg * reg * imm (* rs1, rd, imm *)
	(* load *)
	| Lw of reg * reg * imm (* rs1, rd, offset *)
	| Lre of reg (* rd *)
	| Lrd of reg (* rd *)
	| Ltf of reg (* rd *)
	(* load_fp *)
	| Flw of reg * reg * imm (* rs1, rd, offset *)
	(* jalr *)
	| Jalr of reg * reg (* rs1, rd *)
	(* jal *)
	| Jal of reg * string (* rd, label *)
	(* lui *)
	| Lui of reg * imm (* rd, imm *)
	(* itof *)
	| Fmvif of reg * reg (* rs1, rd *)
	(* ftoi *)
	| Fmvfi of reg * reg (* rs1, rd *)

type code =
	| Labels of string list
	| Operation of operation * int * (string option) (* 命令, 元ファイルでの行番号, (ある場合は)ブレークポイント名 *)

(* opcode *)
let opcode_op = 0
let opcode_op_mfp = 1
let opcode_op_pfp = 2
let opcode_branch = 3
let opcode_branch_fp = 4
let opcode_store = 5
let opcode_store_fp = 6
let opcode_op_imm = 7
let opcode_load = 8
let opcode_load_fp = 9
let opcode_jalr = 10
let opcode_jal = 11
let opcode_lui = 12
let opcode_itof = 13
let opcode_ftoi = 14

(* funct *)
let funct_add = 0
let funct_sub = 1
let funct_sll = 2
let funct_srl = 3
let funct_sra = 4
let funct_and = 5

let funct_fabs = 0
let funct_fneg = 1
let funct_fdiv = 3
let funct_fsqrt = 4
let funct_fcvtif = 5
let funct_fcvtfi = 6
let funct_fmvff = 7

let funct_fadd = 0
let funct_fsub = 1
let funct_fmul = 2

let funct_beq = 0
let funct_blt = 1

let funct_fbeq = 2
let funct_fblt = 3

let funct_sw = 0
let funct_si = 1
let funct_std = 2

let funct_fsw = 0

let funct_addi = 0
let funct_slli = 2
let funct_srli = 3
let funct_srai = 4
let funct_andi = 5

let funct_lw = 0
let funct_lre = 1
let funct_lrd = 2
let funct_ltf = 3

let funct_flw = 0
let funct_jalr = 0
let funct_jal = 0
let funct_lui = 0
let funct_fmvif = 0
let funct_fmvfi = 0
