%{
	open Syntax
	let current_line () = (Parsing.symbol_start_pos ()).pos_lnum		
%}

%token <int> INT
%token <string> ID
%token <string> LABEL
%token LPAR RPAR COLON COMMA PERIOD MINUS EXCLAM EOF
%token INTREG FLOATREG
%token ADD SUB FADD FSUB FMUL FDIV SLL SRL SRA BEQ BLT BLE SW ADDI SLLI SRLI SRAI LW JALR JAL LUI AUIPC

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

operation: // 命令とその行番号の組を返す
	| ADD reg COMMA reg COMMA reg { (Add ($4, $6, $2), current_line ()) } // add rd,rs1,rs2
	| SUB reg COMMA reg COMMA reg { (Sub ($4, $6, $2), current_line ()) } // sub rd,rs1,rs2
	| FADD reg COMMA reg COMMA reg { (Fadd ($4, $6, $2), current_line ()) } // fadd rd,rs1,rs2
	| FSUB reg COMMA reg COMMA reg { (Fsub ($4, $6, $2), current_line ()) } // fsub rd,rs1,rs2
	| FMUL reg COMMA reg COMMA reg { (Fmul ($4, $6, $2), current_line ()) } // fmul rd,rs1,rs2
	| FDIV reg COMMA reg COMMA reg { (Fdiv ($4, $6, $2), current_line ()) } // fdiv rd,rs1,rs2
	| SLL reg COMMA reg COMMA reg { (Sll ($4, $6, $2), current_line ()) } // sll rd,rs1,rs2
	| SRL reg COMMA reg COMMA reg { (Srl ($4, $6, $2), current_line ()) } // srl rd,rs1,rs2
	| SRA reg COMMA reg COMMA reg { (Sra ($4, $6, $2), current_line ()) } // sra rd,rs1,rs2
	| BEQ reg COMMA reg COMMA label { (Beq ($2, $4, $6), current_line ()) } // beq rs1,rs2,label
	| BLT reg COMMA reg COMMA label { (Blt ($2, $4, $6), current_line ()) } // blt rs1,rs2,label
	| BLE reg COMMA reg COMMA label { (Ble ($2, $4, $6), current_line ()) } // ble rs1,rs2,label
	| SW reg COMMA integer LPAR reg RPAR { (Sw ($6, $2, $4), current_line ()) } // sw rs2,offset(rs1)
	| ADDI reg COMMA reg COMMA integer { (Addi ($4, $2, $6), current_line ()) } // addi rd,rs1,imm
	| SLLI reg COMMA reg COMMA integer { (Slli ($4, $2, $6), current_line ()) } // slli rd,rs1,imm
	| SRLI reg COMMA reg COMMA integer { (Srli ($4, $2, $6), current_line ()) } // srli rd,rs1,imm
	| SRAI reg COMMA reg COMMA integer { (Srai ($4, $2, $6), current_line ()) } // srai rd,rs1,imm
	| LW reg COMMA integer LPAR reg RPAR { (Lw ($6, $2, $4), current_line ()) } // lw rd,offset(rs1)
	| JALR reg COMMA reg COMMA integer { (Jalr ($4, $2, $6), current_line ()) } // jalr rd,rs,offset
	| JAL reg COMMA label { (Jal ($2, $4), current_line ()) } // jal rd,label
	| LUI reg COMMA integer { (Lui($2, $4), current_line ()) } // lui rd,imm
	| AUIPC reg COMMA integer { (Auipc($2, $4), current_line ()) } // auipc rd,imm
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
