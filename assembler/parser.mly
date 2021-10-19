%{
	open Syntax
	let current_line () = (Parsing.symbol_start_pos ()).pos_lnum	
%}

%token <int> INT
%token <string> ID
%token <string> LABEL
%token LPAR RPAR COLON COMMA PERIOD MINUS EXCLAM EOF
%token INTREG FLOATREG
%token ADD SUB FADD FSUB FMUL FDIV SLL SRL SRA BEQ BLT FBEQ FBLT BLE SW FSW ADDI SLLI SRLI SRAI LW FLW JALR JAL LUI AUIPC FMVIF FCVTIF FMVFI FCVTFI

%start toplevel
%type <Syntax.code list> toplevel
%%

toplevel:
	| code_list EOF { $1 }
	| error {
		let pos_start = Parsing.symbol_start_pos () in
		let start = pos_start.pos_cnum - pos_start.pos_bol in
			failwith(
				"parse error " ^
				"near line " ^ string_of_int (current_line ()) ^ ", " ^
				"position " ^ string_of_int start
			)
	}

code_list:
	| operation code_list { let (op, line_no) = $1 in Operation (op, line_no, None) :: $2 }
	| label COLON operation code_list { let (op, line_no) = $3 in Label $1 :: Operation (op, line_no, None) :: $4 }
	| operation { let (op, line_no) = $1 in Operation (op, line_no, None) :: [] }
	| label COLON operation { let (op, line_no) = $3 in Label $1 ::  Operation (op, line_no, None) :: [] }
	// with breakpoints
	| operation EXCLAM label code_list { let (op, line_no) = $1 in Operation (op, line_no, Some $3) :: $4 }
	| label COLON operation EXCLAM label code_list { let (op, line_no) = $3 in Label $1 :: Operation (op, line_no, Some $5) :: $6 }
	| label COLON EXCLAM operation code_list { let (op, line_no) = $4 in Label $1 :: Operation (op, line_no, Some $1) :: $5 }
	| operation EXCLAM label { let (op, line_no) = $1 in Operation (op, line_no, Some $3) :: [] }
	| label COLON operation EXCLAM label { let (op, line_no) = $3 in Label $1 :: Operation (op, line_no, Some $5) :: [] }
	| label COLON EXCLAM operation { let (op, line_no) = $4 in Label $1 :: Operation (op, line_no, Some $1) :: [] }
	// | PERIOD ID {}
;

operation:
	| operation_ { ($1, current_line ()) }

operation_: // 命令とその行番号の組を返す
	| ADD reg COMMA reg COMMA reg { Add ($4, $6, $2) } // add rd,rs1,rs2
	| SUB reg COMMA reg COMMA reg { Sub ($4, $6, $2) } // sub rd,rs1,rs2
	| FADD reg COMMA reg COMMA reg { Fadd ($4, $6, $2) } // fadd rd,rs1,rs2
	| FSUB reg COMMA reg COMMA reg { Fsub ($4, $6, $2) } // fsub rd,rs1,rs2
	| FMUL reg COMMA reg COMMA reg { Fmul ($4, $6, $2) } // fmul rd,rs1,rs2
	| FDIV reg COMMA reg COMMA reg { Fdiv ($4, $6, $2) } // fdiv rd,rs1,rs2
	| SLL reg COMMA reg COMMA reg { Sll ($4, $6, $2) } // sll rd,rs1,rs2
	| SRL reg COMMA reg COMMA reg { Srl ($4, $6, $2) } // srl rd,rs1,rs2
	| SRA reg COMMA reg COMMA reg { Sra ($4, $6, $2) } // sra rd,rs1,rs2
	| BEQ reg COMMA reg COMMA label { Beq ($2, $4, $6) } // beq rs1,rs2,label
	| BLT reg COMMA reg COMMA label { Blt ($2, $4, $6) } // blt rs1,rs2,label
	| BLE reg COMMA reg COMMA label { Ble ($2, $4, $6) } // ble rs1,rs2,label
	| FBEQ reg COMMA reg COMMA label { Fbeq ($2, $4, $6) } // fbeq rs1,rs2,label
	| FBLT reg COMMA reg COMMA label { Fblt ($2, $4, $6) } // fblt rs1,rs2,label
	| SW reg COMMA integer LPAR reg RPAR { Sw ($6, $2, $4) } // sw rs2,offset(rs1)
	| FSW reg COMMA integer LPAR reg RPAR { Fsw ($6, $2, $4) } // fsw rs2,offset(rs1)
	| ADDI reg COMMA reg COMMA integer { Addi ($4, $2, $6) } // addi rd,rs1,imm
	| SLLI reg COMMA reg COMMA integer { Slli ($4, $2, $6) } // slli rd,rs1,imm
	| SRLI reg COMMA reg COMMA integer { Srli ($4, $2, $6) } // srli rd,rs1,imm
	| SRAI reg COMMA reg COMMA integer { Srai ($4, $2, $6) } // srai rd,rs1,imm
	| LW reg COMMA integer LPAR reg RPAR { Lw ($6, $2, $4) } // lw rd,offset(rs1)
	| FLW reg COMMA integer LPAR reg RPAR { Flw ($6, $2, $4) } // flw rd,offset(rs1)
	| JALR reg COMMA reg COMMA integer { Jalr ($4, $2, $6) } // jalr rd,rs,offset
	| JAL reg COMMA label { Jal ($2, $4) } // jal rd,label
	| LUI reg COMMA integer { Lui($2, $4) } // lui rd,imm
	| AUIPC reg COMMA integer { Auipc($2, $4) } // auipc rd,imm
	| FMVIF reg COMMA reg { Fmvif ($4, $2) } // fmv.i.f rd,rs1
	| FCVTIF reg COMMA reg { Fcvtif ($4, $2) } // fcvt.i.f rd,rs1
	| FMVFI reg COMMA reg { Fmvfi ($4, $2) } // fmv.f.i rd,rs1
	| FCVTFI reg COMMA reg { Fcvtfi ($4, $2) } // fcvt.f.i rd,rs1
;

reg:
	| INTREG INT { Int_reg $2 }
	| FLOATREG INT { Float_reg $2 }
;

label:
	| ID { $1 }
	| LABEL { $1 }
;

integer:
	| INT { $1 }
	| MINUS INT { -$2 }
;
