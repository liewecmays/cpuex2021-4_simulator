type reg =
	| Int_reg of int
	| Float_reg of int

type imm =
	| Dec of int (* 10進数 *)
	| Hex of string (* 16進数 *)
	| Neghex of string (* マイナスの付いた16進数 *)
	| Label of string (* ラベル *)

type operation =
	| Add of reg * reg * reg (* rs1, rs2, rd *)
	| Sub of reg * reg * reg (* rs1, rs2, rd *)
	| And of reg * reg * reg (* rs1, rs2, rd *)
	| Fadd of reg * reg * reg (* rs1, rs2, rd *)
	| Fsub of reg * reg * reg (* rs1, rs2, rd *)
	| Fmul of reg * reg * reg (* rs1, rs2, rd *)
	| Fsqrt of reg * reg (* rs1, rd *)
	| Fdiv of reg * reg * reg (* rs1, rs2, rd *)
	| Sll of reg * reg * reg (* rs1, rs2, rd *)
	| Srl of reg * reg * reg (* rs1, rs2, rd *)
	| Sra of reg * reg * reg (* rs1, rs2, rd *)
	| Beq of reg * reg * string (* rs1, rs2, label *)
	| Blt of reg * reg * string (* rs1, rs2, label *)
	| Ble of reg * reg * string (* rs1, rs2, label *)
	| Fbeq of reg * reg * string (* rs1, rs2, label *)
	| Fblt of reg * reg * string (* rs1, rs2, label *)
	| Sw of reg * reg * imm (* rs1, rs2, offset *)
	| Si of reg * reg * imm (* rs1, rs2, offset *)
	| Std of reg (* rs1 *)
	| Fsw of reg * reg * imm (* rs1, rs2, offset *)
	| Fstd of reg (* rs1 *)
	| Addi of reg * reg * imm (* rs1, rd, imm *)
	| Slli of reg * reg * imm (* rs1, rd, imm *)
	| Srli of reg * reg * imm (* rs1, rd, imm *)
	| Andi of reg * reg * imm (* rs1, rd, imm *)
	| Srai of reg * reg * imm (* rs1, rd, imm *)
	| Lw of reg * reg * imm (* rs1, rd, offset *)
	| Lre of reg (* rd *)
	| Lrd of reg (* rd *)
	| Ltf of reg (* rd *)
	| Flw of reg * reg * imm (* rs1, rd, offset *)
	| Flrd of reg (* rs1 *)
	| Jalr of reg * reg * imm (* rs1, rd, offset *)
	| Jal of reg * string (* rd, label *)
	| Lui of reg * imm (* rd, imm *)
	| Auipc of reg * imm (* rd, imm *)
	| Fmvif of reg * reg (* rs1, rd *)
	| Fcvtif of reg * reg (* rs1, rd *)
	| Fmvfi of reg * reg (* rs1, rd *)
	| Fcvtfi of reg * reg (* rs1, rd *)
	| Floor of reg * reg (* rs1, rd *)

type code =
	| Labels of string list
	| Operation of operation * int * (string option) (* 命令, 元ファイルでの行番号, (ある場合は)ブレークポイント名 *)
