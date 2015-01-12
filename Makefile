export PROJ_ROOT=$(CURDIR)
export ARCH_BITS=$(shell getconf LONG_BIT)
all:
	@echo $(PROJ_ROOT)
	rm -r -f bin/ obj/ lib/
	mkdir bin
	mkdir obj
	mkdir lib
	cd crt; make
	cd clients; make
	cd driver; make
clean:
	cd crt; make clean
	cd clients; make clean
	cd driver; make clean
	rm -rf bin/ obj/ lib/ *.log
