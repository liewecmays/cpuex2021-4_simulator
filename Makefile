default: # assembler + all simulators
	$(MAKE) -C assembler
	$(MAKE) -C simulator

all:
	$(MAKE) -C assembler
	$(MAKE) all -C simulator

1st: # assembler + 1st simulator
	$(MAKE) -C assembler
	$(MAKE) sim -C simulator

1st-d: # assembler + extended 1st simulator
	$(MAKE) -C assembler
	$(MAKE) sim-d -C simulator

2nd: # assembler + 2nd simulator
	$(MAKE) -C assembler
	$(MAKE) sim2 -C simulator

fpu_test:
	$(MAKE) fpu_test -C simulator

server:
	$(MAKE) server -C simulator

clean:
	$(MAKE) clean -C assembler
	$(MAKE) clean -C simulator

clean-all:
	$(MAKE) clean -C assembler
	$(MAKE) clean-all -C simulator
