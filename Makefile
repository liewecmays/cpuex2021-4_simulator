all:
	$(MAKE) -C assembler
	$(MAKE) -C simulator

clean:
	$(MAKE) clean -C assembler
	$(MAKE) clean -C simulator