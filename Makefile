export PROJ_ROOT=$(CURDIR)
export ARCH_BITS=$(shell getconf LONG_BIT)
all:
	@echo $(PROJ_ROOT)
	rm -r -f bin/ objs/ libs/
	mkdir bin
	mkdir objs
	mkdir libs
	cd crtlib; make
	cd utils; make
	cd driver; make
clean:
	cd crtlib; make clean
	cd utils; make clean
	cd driver; make clean
	rm -r -f bin/ objs/ libs/ *.log
