type reg =
	| Int_reg of int
	| Float_reg of int

type operation =
	| Add of reg * reg * reg (* rs1, rs2, rd *)
	| Sub of reg * reg * reg (* rs1, rs2, rd *)
	| Sll of reg * reg * reg (* rs1, rs2, rd *)
	| Srl of reg * reg * reg (* rs1, rs2, rd *)
	| Sra of reg * reg * reg (* rs1, rs2, rd *)
	| Beq of reg * reg * string (* rs1, rs2, label *)
	| Blt of reg * reg * string (* rs1, rs2, label *)
	| Ble of reg * reg * string (* rs1, rs2, label *)
	| Sw of reg * reg * int (* rs1, rs2, offset *)
	| Addi of reg * reg * int (* rs1, rd, imm *)
	| Slli of reg * reg * int (* rs1, rd, imm *)
	| Srli of reg * reg * int (* rs1, rd, imm *)
	| Srai of reg * reg * int (* rs1, rd, imm *)
	| Lw of reg * reg * int (* rs1, rd, offset *)
	| Jalr of reg * reg * int (* rs1, rd, offset *)
	| Jal of reg * string (* rd, label *)
	| Lui of reg * int (* rd, imm *)
	| Auipc of reg * int (* rd, imm *)

type code =
	| Label of string
	| Operation of operation * (string option) (* 命令, (ある場合は)ブレークポイント名 *)
