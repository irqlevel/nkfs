all:
	cd driver; make
	cd utils; make
clean:
	cd driver; make clean
	cd utils; make clean

