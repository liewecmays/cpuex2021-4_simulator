type reg =
	| Int_reg of int
	| Float_reg of int

type code =
	| Label of string
	| Operation of operation

and operation =
	| Add of reg * reg * reg (* rs1, rs2, rd *)
	| Sub of reg * reg * reg (* rs1, rs2, rd *)
	| Beq of reg * reg * string (* rs1, rs2, label *)
	| Blt of reg * reg * string (* rs1, rs2, label *)
	| Ble of reg * reg * string (* rs1, rs2, label *)
	| Sw of reg * reg * int (* rs1, rs2, offset *)
	| Addi of reg * reg * int (* rs1, rd, imm *)
	| Lw of reg * reg * int (* rs1, rd, offset *)
	| Jal of reg * string (* rd, label *)
