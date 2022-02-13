default:
	$(MAKE) -C assembler
	$(MAKE) -C simulator

1st:
	$(MAKE) -C assembler
	$(MAKE) sim -C simulator

1st-d:
	$(MAKE) -C assembler
	$(MAKE) sim-d -C simulator

2nd:
	$(MAKE) -C assembler
	$(MAKE) sim2 -C simulator

all:
	$(MAKE) -C assembler
	$(MAKE) all -C simulator

clean:
	$(MAKE) clean -C assembler
	$(MAKE) clean -C simulator

clean-all:
	$(MAKE) clean -C assembler
	$(MAKE) clean-all -C simulator
