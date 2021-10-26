asm: syntax.ml parser.mly lexer.mll main.ml
	ocamllex lexer.mll -q
	ocamlyacc -v parser.mly
	ocamlopt -o asm str.cmxa syntax.ml parser.mli parser.ml lexer.ml main.ml

clean:
	rm -f parser.ml parser.mli lexer.ml asm *.cmi *.cmo *.mli *.o *.cmx *.output