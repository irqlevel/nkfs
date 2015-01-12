export PROJ_ROOT=$(CURDIR)
export ARCH_BITS=$(shell getconf LONG_BIT)
all:
	@echo $(PROJ_ROOT)
	rm -r -f bin/ obj/ lib/
	mkdir bin
	mkdir obj
	mkdir lib
	cd crt; make
	cd client; make
	cd ctl;	make
	cd kmod; make
clean:
	cd crt; make clean
	cd client; make clean
	cd ctl; make clean
	cd kmod; make clean
	rm -rf bin/ obj/ lib/ *.log
