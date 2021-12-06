let digit = ['0'-'9']
let hex = "0x" (['0'-'9' 'a'-'f' 'A'-'F'])+
let space = ' ' | '\t' | '\r'
let newline = '\n'
let alpha = ['a'-'z' 'A'-'Z' '_' ]
let ident = alpha (alpha | digit)*
let label = alpha (alpha | digit)* ('.' (digit)+)*

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
(* op *)
| "add" { Parser.ADD }
| "sub" { Parser.SUB }
| "sll" { Parser.SLL }
| "srl" { Parser.SRL }
| "sra" { Parser.SRA }
| "and" { Parser.AND }
(* op_fp *)
| "fadd" { Parser.FADD }
| "fsub" { Parser.FSUB }
| "fmul" { Parser.FMUL }
| "fdiv" { Parser.FDIV }
| "fsqrt" { Parser.FSQRT }
| "fcvt.i.f" { Parser.FCVTIF }
| "fcvt.f.i" { Parser.FCVTFI }
(* branch *)
| "beq" { Parser.BEQ }
| "blt" { Parser.BLT }
(* branch_fp *)
| "fbeq" { Parser.FBEQ }
| "fblt" { Parser.FBLT }
(* store *)
| "sw" { Parser.SW }
| "si" { Parser.SI }
| "std" { Parser.STD }
(* store_fp *)
| "fsw" { Parser.FSW }
(* op_fp *)
| "addi" { Parser.ADDI }
| "slli" { Parser.SLLI }
| "srli" { Parser.SRLI }
| "srai" { Parser.SRAI }
| "andi" { Parser.ANDI }
(* load *)
| "lw" { Parser.LW }
| "lre" { Parser.LRE }
| "lrd" { Parser.LRD }
| "ltf" { Parser.LTF }
(* load_fp *)
| "flw" { Parser.FLW }
(* jald *)
| "jalr" { Parser.JALR }
(* jal *)
| "jal" { Parser.JAL }
(* lui *)
| "lui" { Parser.LUI }
(* itof *)
| "fmv.i.f" { Parser.FMVIF }
(* ftoi *)
| "fmv.f.i" { Parser.FMVFI }
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
