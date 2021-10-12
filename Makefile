all:
	$(MAKE) -C assembler
	$(MAKE) -C simulator

.PHONY: clean
clean:
	$(MAKE) clean -C assembler
	$(MAKE) clean -C simulator