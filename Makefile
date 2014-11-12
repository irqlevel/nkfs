export PROJ_ROOT=$(CURDIR)
all:
	@echo $(PROJ_ROOT)
	rm -r -f bin/ objs/
	mkdir bin
	mkdir objs
	cd libs; make
	cd dsmod; make
	cd utils; make
clean:
	cd libs; make clean
	cd dsmod; make clean
	cd utils; make clean
	rm -r -f bin/ objs/
