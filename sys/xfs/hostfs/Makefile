#
# the m68k arch specific files
#
TARGET = hostfs.xfs

SHELL = /bin/sh
SUBDIRS = 

srcdir = .
top_srcdir = ../..
subdir = hostfs

default: all

include $(top_srcdir)/CONFIGVARS
include $(top_srcdir)/RULES
include $(top_srcdir)/PHONY

all-here: build

ifneq ($(kernel),)

build: $(compile_dir)
	cd $(compile_dir); $(MAKE) all

else

build:
	$(MAKE) -f Makefile.xfs all

endif

# default overwrites

# default definitions
GENFILES = $(compile_all_dirs)
GENFILES += $(TARGET)
