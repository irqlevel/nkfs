export PROJ_ROOT=$(CURDIR)
all:
	@echo $(PROJ_ROOT)
	rm -r -f bin/ objs/ libs/
	mkdir bin
	mkdir objs
	mkdir libs
	cd crtlib; make
	cd driver; make
	cd utils; make
clean:
	cd crtlib; make clean
	cd driver; make clean
	cd utils; make clean
	rm -r -f bin/ objs/ libs/
