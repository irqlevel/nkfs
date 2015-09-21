include $(PROJ_ROOT)/Makefile.inc

SUBDIRS = kernel user
SUBCLEAN = $(addsuffix .clean,$(SUBDIRS))

.PHONY: all clean $(SUBDIRS) $(SUBCLEAN)

all: $(SUBDIRS)
clean: curr_dir_clean $(SUBCLEAN)

curr_dir_clean:
	rm -rf *.o *.a && find . -name '.*' -type f | xargs rm -f

$(SUBDIRS):
	$(MAKE) -C $@

$(SUBCLEAN): %.clean:
	$(MAKE) -C $* clean
