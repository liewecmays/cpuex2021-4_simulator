asm: syntax.ml parser.mly lexer.mll util.ml main.ml
	ocamllex lexer.mll -q
	ocamlyacc parser.mly
	ocamlopt -o asm syntax.ml parser.mli parser.ml lexer.ml util.ml main.ml

clean:
	rm -f parser.ml parser.mli lexer.ml asm *.cmi *.cmo *.mli *.o *.cmx *.output