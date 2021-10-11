let digit = ['0'-'9']
let space = ' ' | '\t' | '\r' | '\n'
let alpha = ['a'-'z' 'A'-'Z' '_' ] 
let ident = alpha (alpha | digit)*

rule token = parse
| space+ { token lexbuf }
| "(" { Parser.LPAR }
| ")" { Parser.RPAR }
| ":" { Parser.COLON }
| "," { Parser.COMMA }
| "." { Parser.PERIOD }
| "-" { Parser.MINUS }
| "add" { Parser.ADD }
| "sub" { Parser.SUB }
| "beq" { Parser.BEQ }
| "blt" { Parser.BLT }
| "ble" { Parser.BLE }
| "sw" { Parser.SW }
| "addi" { Parser.ADDI }
| "lw" { Parser.LW }
| "jal" { Parser.JAL }
| digit+ as n  { Parser.INT (int_of_string n) }
| ident as id { Parser.ID id }
| "#" { comment lexbuf; token lexbuf }
| "!" { comment lexbuf; token lexbuf }
| eof { Parser.EOF }
| _ { failwith ("lex error") }

and comment = parse
| '\n' { () }
| _ { comment lexbuf }
(* note: 最後の行がコメント付きだとエラーを起こす(最後の行は空行にした方が良い) *)