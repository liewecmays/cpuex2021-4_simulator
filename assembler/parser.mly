%{
	open Syntax
%}

%token <int> INT
%token <string> ID
%token LPAR RPAR COLON COMMA PERIOD MINUS EOF
%token INTREG FLOATREG
%token ADD SUB BEQ BLT BLE SW ADDI LW JALR JAL

%start toplevel
%type <Syntax.code list> toplevel
%%

toplevel:
	| code_list EOF { $1 }

code_list:
	| operation code_list { Operation $1 :: $2 }
	| ID COLON operation code_list { Label $1 ::  Operation $3 :: $4 }
	| operation { Operation($1) :: [] }
	| ID COLON operation { Label $1 ::  Operation $3 :: [] }
	// | PERIOD ID {}
;

operation:
	| ADD reg COMMA reg COMMA reg { Add ($4, $6, $2) } // add rd,rs1,rs2
	| SUB reg COMMA reg COMMA reg { Sub ($4, $6, $2) } // sub rd,rs1,rs2
	| BEQ reg COMMA reg COMMA ID { Beq ($2, $4, $6) } // beq rs1,rs2,label
	| BLT reg COMMA reg COMMA ID { Blt ($2, $4, $6) } // blt rs1,rs2,label
	| BLE reg COMMA reg COMMA ID { Ble ($2, $4, $6) } // ble rs1,rs2,label
	| SW reg COMMA integer LPAR reg RPAR { Sw ($6, $2, $4) } // sw rs2,offset(rs1)
	| ADDI reg COMMA reg COMMA integer { Addi ($4, $2, $6) } // addi rd,rs1,imm
	| LW reg COMMA integer LPAR reg RPAR { Lw ($6, $2, $4) } // lw rd,offset(rs1)
	| JALR reg COMMA reg COMMA integer { Jalr ($4, $2, $6) } // jalr rd,rs,offset
	| JAL reg COMMA ID { Jal ($2, $4) } // jal rd,label
;

reg:
	| INTREG INT { Int_reg $2 }
	| FLOATREG INT { Float_reg $2 }
;

integer:
	| INT { $1 }
	| MINUS INT { -$2 }
;
