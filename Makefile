export PROJ_ROOT=$(CURDIR)
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
	rm -r -f bin/ objs/ libs/
