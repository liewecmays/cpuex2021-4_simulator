asm: syntax.ml parser.mly lexer.mll util.ml main.ml
	ocamllex lexer.mll -q
	ocamlyacc -v parser.mly
	ocamlopt -o asm str.cmxa syntax.ml parser.mli parser.ml lexer.ml util.ml main.ml

clean:
	rm -f parser.ml parser.mli lexer.ml asm *.cmi *.cmo *.mli *.o *.cmx *.output