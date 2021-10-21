let digit = ['0'-'9']
let hex = "0x" (['0'-'9' 'a'-'f' 'A'-'F'])+
let space = ' ' | '\t' | '\r'
let newline = '\n'
let alpha = ['a'-'z' 'A'-'Z' '_' ]
let ident = alpha (alpha | digit)*
let label = alpha (alpha | digit)* ('.' (digit)+)

rule token = parse
| space+ { token lexbuf }
| newline { Lexing.new_line lexbuf; token lexbuf }
| "(" { Parser.LPAR }
| ")" { Parser.RPAR }
| ":" { Parser.COLON }
| "," { Parser.COMMA }
| "." { Parser.PERIOD }
| "-" { Parser.MINUS }
| "!" { Parser.EXCLAM }
| "add" { Parser.ADD }
| "sub" { Parser.SUB }
| "and" { Parser.AND }
| "fadd" { Parser.FADD }
| "fsub" { Parser.FSUB }
| "fmul" { Parser.FMUL }
| "fdiv" { Parser.FDIV }
| "sll" { Parser.SLL }
| "srl" { Parser.SRL }
| "sra" { Parser.SRA }
| "beq" { Parser.BEQ }
| "blt" { Parser.BLT }
| "ble" { Parser.BLE }
| "fbeq" { Parser.FBEQ }
| "fblt" { Parser.FBLT }
| "sw" { Parser.SW }
| "fsw" { Parser.FSW }
| "addi" { Parser.ADDI }
| "slli" { Parser.SLLI }
| "srli" { Parser.SRLI }
| "srai" { Parser.SRAI }
| "andi" { Parser.ANDI }
| "lw" { Parser.LW }
| "flw" { Parser.FLW }
| "jalr" { Parser.JALR }
| "jal" { Parser.JAL }
| "lui" { Parser.LUI }
| "auipc" { Parser.AUIPC }
| "fmv.i.f" { Parser.FMVIF }
| "fcvt.i.f" { Parser.FCVTIF }
| "fmv.f.i" { Parser.FMVFI }
| "fcvt.f.i" { Parser.FCVTFI }
| "%x" { Parser.INTREG }
| "%f" { Parser.FLOATREG }
| digit+ as n  { Parser.INT (int_of_string n) }
| hex as h { Parser.HEX (Str.string_after h 2) }
| ident as id { Parser.ID id }
| label as l { Parser.LABEL l }
| "#" { comment lexbuf; token lexbuf }
| eof { Parser.EOF }
| _ {
    let start_pos = Lexing.lexeme_start_p lexbuf in
        failwith (
            "unknown token " ^
            "'" ^ (Lexing.lexeme lexbuf) ^ "'" ^
            " (in line " ^ (string_of_int start_pos.pos_lnum) ^
            ", at position " ^ (string_of_int (start_pos.pos_cnum - start_pos.pos_bol)) ^ ")"
        )
}

and comment = parse
| '\n' { Lexing.new_line lexbuf; () }
| _ { comment lexbuf }
(* note: 最後の行がコメント付きだとエラーを起こす(最後の行は空行にした方が良い) *)
