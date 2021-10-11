%{
	open Syntax
%}

%token <int> INT
%token <string> ID
%token LPAR RPAR COLON COMMA PERIOD MINUS EOF
%token ADD SUB BEQ BLT BLE SW ADDI LW JAL

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
	| ADD ID COMMA ID COMMA ID { Add ($4, $6, $2) } // add rd,rs1,rs2
	| SUB ID COMMA ID COMMA ID { Sub ($4, $6, $2) } // sub rd,rs1,rs2
	| BEQ ID COMMA ID COMMA ID { Beq ($2, $4, $6) } // beq rs1,rs2,label
	| BLT ID COMMA ID COMMA ID { Blt ($2, $4, $6) } // blt rs1,rs2,label
	| BLE ID COMMA ID COMMA ID { Ble ($2, $4, $6) } // ble rs1,rs2,label
	| SW ID COMMA integer LPAR ID RPAR { Sw ($6, $2, $4) } // sw rs2,offset(rs1)
	| ADDI ID COMMA ID COMMA integer { Addi ($4, $2, $6) } // addi rd,rs1,imm
	| LW ID COMMA integer LPAR ID RPAR { Lw ($6, $2, $4) } // lw rd,offset(rs1)
	| JAL ID COMMA ID { Jal ($2, $4) } // jal rd,label
;

integer:
	| INT { $1 }
	| MINUS INT { -$2 }
;
