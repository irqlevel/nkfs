include $(PROJ_ROOT)/Makefile.inc

SUBDIRS = kernel user
SUBCLEAN = $(addsuffix .clean,$(SUBDIRS))

.PHONY: all clean $(SUBDIRS) $(SUBCLEAN)

all: $(SUBDIRS)
clean: $(SUBCLEAN)

$(SUBDIRS):
	$(MAKE) -C $@

$(SUBCLEAN): %.clean:
	$(MAKE) -C $* clean
