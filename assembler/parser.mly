%{
	open Syntax
	let current_line () = (Parsing.symbol_start_pos ()).pos_lnum	
%}

%token <int> INT
%token <string> HEX
%token <string> ID
%token <string> LABEL
%token LPAR RPAR COLON COMMA PERIOD MINUS EXCLAM EOF
%token INTREG FLOATREG
%token ADD SUB SLL SRL SRA AND
%token FADD FSUB FMUL
%token FABS FNEG FDIV FSQRT FCVTIF FCVTFI FMVFF
%token BEQ BLT
%token FBEQ FBLT
%token SW SI STD
%token FSW
%token ADDI SLLI SRLI SRAI ANDI
%token LW LRE LRD LTF
%token FLW
%token JALR
%token JAL
%token LUI
%token FMVIF
%token FMVFI

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
;

code_list:
	| operation code_list { let (op, line_no) = $1 in Operation (op, line_no, None) :: $2 }
	| label_list operation code_list { let (op, line_no) = $2 in Labels $1 :: Operation (op, line_no, None) :: $3 }
	| operation { let (op, line_no) = $1 in Operation (op, line_no, None) :: [] }
	| label_list operation { let (op, line_no) = $2 in Labels $1 ::  Operation (op, line_no, None) :: [] }
	// with breakpoints
	| operation EXCLAM label code_list { let (op, line_no) = $1 in Operation (op, line_no, Some $3) :: $4 }
	// | label_list operation EXCLAM label code_list { let (op, line_no) = $2 in Labels $1 :: Operation (op, line_no, Some $4) :: $5 }
	| label_list EXCLAM operation code_list
		{
			let bp = List.hd (List.rev $1) in
			let (op, line_no) = $3 in Labels $1 :: Operation (op, line_no, Some bp) :: $4
		}
	| operation EXCLAM label { let (op, line_no) = $1 in Operation (op, line_no, Some $3) :: [] }
	// | label_list operation EXCLAM label { let (op, line_no) = $3 in Labels $1 :: Operation (op, line_no, Some $5) :: [] }
	| label_list EXCLAM operation
		{
			let bp = List.hd (List.rev $1) in
			let (op, line_no) = $3 in Labels $1 :: Operation (op, line_no, Some bp) :: [] }
;

label_list:
	| label COLON label_list { $1 :: $3 }
	| label COLON { $1 :: [] }
;

operation:
	| operation_ { ($1, current_line ()) }
;

operation_: // ???????????????????????????????????????
	// op
	| ADD reg COMMA reg COMMA reg { Add ($4, $6, $2) } // add rd,rs1,rs2
	| SUB reg COMMA reg COMMA reg { Sub ($4, $6, $2) } // sub rd,rs1,rs2
	| SLL reg COMMA reg COMMA reg { Sll ($4, $6, $2) } // sll rd,rs1,rs2
	| SRL reg COMMA reg COMMA reg { Srl ($4, $6, $2) } // srl rd,rs1,rs2
	| SRA reg COMMA reg COMMA reg { Sra ($4, $6, $2) } // sra rd,rs1,rs2
	| AND reg COMMA reg COMMA reg { And ($4, $6, $2) } // and rd,rs1,rs2
	// op_mfp
	| FABS reg COMMA reg { Fabs ($4, $2) } // fabs rd,rs1
	| FNEG reg COMMA reg { Fneg ($4, $2) } // fneg rd,rs1
	| FDIV reg COMMA reg COMMA reg { Fdiv ($4, $6, $2) } // fdiv rd,rs1,rs2
	| FSQRT reg COMMA reg { Fsqrt ($4, $2) } // fsqrt rd,rs1
	| FCVTIF reg COMMA reg { Fcvtif ($4, $2) } // fcvt.i.f rd,rs1
	| FCVTFI reg COMMA reg { Fcvtfi ($4, $2) } // fcvt.f.i rd,rs1
	| FMVFF reg COMMA reg { Fmvff ($4, $2) } // fmv.f.f rd,rs1
	// op_pfp
	| FADD reg COMMA reg COMMA reg { Fadd ($4, $6, $2) } // fadd rd,rs1,rs2
	| FSUB reg COMMA reg COMMA reg { Fsub ($4, $6, $2) } // fsub rd,rs1,rs2
	| FMUL reg COMMA reg COMMA reg { Fmul ($4, $6, $2) } // fmul rd,rs1,rs2
	// branch
	| BEQ reg COMMA reg COMMA label { Beq ($2, $4, $6) } // beq rs1,rs2,label
	| BLT reg COMMA reg COMMA label { Blt ($2, $4, $6) } // blt rs1,rs2,label
	// branch_fp
	| FBEQ reg COMMA reg COMMA label { Fbeq ($2, $4, $6) } // fbeq rs1,rs2,label
	| FBLT reg COMMA reg COMMA label { Fblt ($2, $4, $6) } // fblt rs1,rs2,label
	// store
	| SW reg COMMA immediate LPAR reg RPAR { Sw ($6, $2, $4) } // sw rs2,offset(rs1)
	| SI reg COMMA immediate LPAR reg RPAR { Si ($6, $2, $4) } // si rs2,offset(rs1)
	| STD reg { Std $2 } // std rs1
	// store_fp
	| FSW reg COMMA immediate LPAR reg RPAR { Fsw ($6, $2, $4) } // fsw rs2,offset(rs1)
	// op_imm
	| ADDI reg COMMA reg COMMA immediate { Addi ($4, $2, $6) } // addi rd,rs1,imm
	| SLLI reg COMMA reg COMMA immediate { Slli ($4, $2, $6) } // slli rd,rs1,imm
	| SRLI reg COMMA reg COMMA immediate { Srli ($4, $2, $6) } // srli rd,rs1,imm
	| SRAI reg COMMA reg COMMA immediate { Srai ($4, $2, $6) } // srai rd,rs1,imm
	| ANDI reg COMMA reg COMMA immediate { Andi ($4, $2, $6) } // andi rd,rs1,imm
	// load
	| LW reg COMMA immediate LPAR reg RPAR { Lw ($6, $2, $4) } // lw rd,offset(rs1)
	| LRE reg { Lre $2 } // lre rd
	| LRD reg { Lrd $2 } // lrd rd
	| LTF reg { Ltf $2 } // ltf rd
	// load_fp
	| FLW reg COMMA immediate LPAR reg RPAR { Flw ($6, $2, $4) } // flw rd,offset(rs1)
	// jalr
	| JALR reg COMMA reg { Jalr ($4, $2) } // jalr rd,rs1
	// jal
	| JAL reg COMMA label { Jal ($2, $4) } // jal rd,label
	// lui
	| LUI reg COMMA immediate { Lui($2, $4) } // lui rd,imm
	// itof
	| FMVIF reg COMMA reg { Fmvif ($4, $2) } // fmv.i.f rd,rs1
	// ftoi
	| FMVFI reg COMMA reg { Fmvfi ($4, $2) } // fmv.f.i rd,rs1
;

reg:
	| INTREG INT { Int_reg $2 }
	| FLOATREG INT { Float_reg $2 }
;

immediate:
	| integer { Dec $1 }
	| HEX { Hex $1 }
	| MINUS HEX { Neghex $2 }
	| label { Label $1 }
;

label:
	| ID { $1 }
	| LABEL { $1 }
;

integer:
	| INT { $1 }
	| MINUS INT { -$2 }
;
