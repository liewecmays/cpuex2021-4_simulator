let digit = ['0'-'9']
let space = ' ' | '\t' | '\r' | '\n'
let alpha = ['a'-'z' 'A'-'Z' '_' ]
let ident = alpha (alpha | digit)*
let label = alpha (alpha | digit)* ('.' (digit)+)

rule token = parse
| space+ { token lexbuf }
| "(" { Parser.LPAR }
| ")" { Parser.RPAR }
| ":" { Parser.COLON }
| "," { Parser.COMMA }
| "." { Parser.PERIOD }
| "-" { Parser.MINUS }
| "@" { Parser.AT }
| "add" { Parser.ADD }
| "sub" { Parser.SUB }
| "sll" { Parser.SLL }
| "srl" { Parser.SRL }
| "sra" { Parser.SRA }
| "beq" { Parser.BEQ }
| "blt" { Parser.BLT }
| "ble" { Parser.BLE }
| "sw" { Parser.SW }
| "addi" { Parser.ADDI }
| "slli" { Parser.SLLI }
| "srli" { Parser.SRLI }
| "srai" { Parser.SRAI }
| "lw" { Parser.LW }
| "jalr" { Parser.JALR }
| "jal" { Parser.JAL }
| "lui" { Parser.LUI }
| "auipc" { Parser.AUIPC }
| "%x" { Parser.INTREG }
| "%f" { Parser.FLOATREG }
| digit+ as n  { Parser.INT (int_of_string n) }
| ident as id { Parser.ID id }
| label as l { Parser.LABEL l }
| "#" { comment lexbuf; token lexbuf }
| "!" { comment lexbuf; token lexbuf }
| eof { Parser.EOF }
| _ { failwith ("lex error") }

and comment = parse
| '\n' { () }
| _ { comment lexbuf }
(* note: 最後の行がコメント付きだとエラーを起こす(最後の行は空行にした方が良い) *)