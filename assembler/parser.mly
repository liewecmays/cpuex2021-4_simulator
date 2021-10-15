%{
	open Syntax
%}

%token <int> INT
%token <string> ID
%token <string> LABEL
%token LPAR RPAR COLON COMMA PERIOD MINUS AT EOF
%token INTREG FLOATREG
%token ADD SUB SLL SRL SRA BEQ BLT BLE SW ADDI SLLI SRLI SRAI LW JALR JAL LUI AUIPC

%start toplevel
%type <Syntax.code list> toplevel
%%

toplevel:
	| code_list EOF { $1 }

code_list:
	| operation code_list { Operation ($1, None) :: $2 }
	| label COLON operation code_list { Label $1 :: Operation ($3, None) :: $4 }
	| operation { Operation ($1, None) :: [] }
	| label COLON operation { Label $1 ::  Operation ($3, None) :: [] }
	// with breakpoints
	| operation AT label code_list { Operation ($1, Some $3) :: $4 }
	| label COLON operation AT label code_list { Label $1 :: Operation ($3, Some $5) :: $6 }
	| label COLON AT operation code_list { Label $1 :: Operation ($4, Some $1) :: $5 }
	| operation AT label { Operation ($1, Some $3) :: [] }
	| label COLON operation AT label { Label $1 :: Operation ($3, Some $5) :: [] }
	| label COLON AT operation { Label $1 :: Operation ($4, Some $1) :: [] }
	// | PERIOD ID {}
;

operation:
	| ADD reg COMMA reg COMMA reg { Add ($4, $6, $2) } // add rd,rs1,rs2
	| SUB reg COMMA reg COMMA reg { Sub ($4, $6, $2) } // sub rd,rs1,rs2
	| SLL reg COMMA reg COMMA reg { Sll ($4, $6, $2) } // sll rd,rs1,rs2
	| SRL reg COMMA reg COMMA reg { Srl ($4, $6, $2) } // srl rd,rs1,rs2
	| SRA reg COMMA reg COMMA reg { Sra ($4, $6, $2) } // sra rd,rs1,rs2
	| BEQ reg COMMA reg COMMA label { Beq ($2, $4, $6) } // beq rs1,rs2,label
	| BLT reg COMMA reg COMMA label { Blt ($2, $4, $6) } // blt rs1,rs2,label
	| BLE reg COMMA reg COMMA label { Ble ($2, $4, $6) } // ble rs1,rs2,label
	| SW reg COMMA integer LPAR reg RPAR { Sw ($6, $2, $4) } // sw rs2,offset(rs1)
	| ADDI reg COMMA reg COMMA integer { Addi ($4, $2, $6) } // addi rd,rs1,imm
	| SLLI reg COMMA reg COMMA integer { Slli ($4, $2, $6) } // slli rd,rs1,imm
	| SRLI reg COMMA reg COMMA integer { Srli ($4, $2, $6) } // srli rd,rs1,imm
	| SRAI reg COMMA reg COMMA integer { Srai ($4, $2, $6) } // srai rd,rs1,imm
	| LW reg COMMA integer LPAR reg RPAR { Lw ($6, $2, $4) } // lw rd,offset(rs1)
	| JALR reg COMMA reg COMMA integer { Jalr ($4, $2, $6) } // jalr rd,rs,offset
	| JAL reg COMMA label { Jal ($2, $4) } // jal rd,label
	| LUI reg COMMA integer { Lui($2, $4) } // lui rd,imm
	| AUIPC reg COMMA integer { Auipc($2, $4) } // auipc rd,imm
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
