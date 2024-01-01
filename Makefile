LIB = fminc

RHEL_MAJOR_VERSION := $(shell awk -F'[="]' '/VERSION_ID/ { print substr($$3, 1, 1) }' /etc/os-release)

MAINFLAGS = -Wall -W -Wno-unused-parameter -Werror

EXTRAFLAGS = -Wpointer-arith \
	-Wcast-qual \
	-Wcast-align \
	-Wwrite-strings \
	-Wconversion \
	-Winline \
	-Wnon-virtual-dtor \
	-Wno-pmf-conversions \
	-Wsign-promo \
	-Wchar-subscripts \
	-Wold-style-cast

DIFFICULTFLAGS = -pedantic -Weffc++ -Wredundant-decls -Wshadow -Woverloaded-virtual -Wunreachable-code -Wctor-dtor-privacy

CC = /usr/bin/g++

# Default compiler flags

CFLAGS = -fPIC -std=c++17 -DUNIX -O2 -g -DNDEBUG $(MAINFLAGS)
LDFLAGS = -shared

# Special modes

CFLAGS_DEBUG = -fPIC -std=c++17 -DUNIX -O0 -ggdb -DDEBUG $(MAINFLAGS) $(EXTRAFLAGS)

LDFLAGS_DEBUG = -shared -Wl,-soname,lib$(LIB).so

INCLUDES = -I include

ifneq ($(INCLUDE), "")
  INCLUDES := $(INCLUDES) \
		$(INCLUDE)
endif

LIBDIRS =
LIBS = -lboost_filesystem -lfmt

ifeq ($(RHEL_MAJOR_VERSION),8)
  INCLUDES := $(INCLUDES) -isystem /usr/include/boost169
  LIBDIRS = -L/usr/lib64/boost169
endif


# Common library compiling template

# Installation directories

processor := $(shell uname -p)

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

ifeq ($(processor), x86_64)
  LIBDIR = $(PREFIX)/lib64
else
  LIBDIR = $(PREFIX)/lib
endif

objdir = obj
LIBDIR = lib

includedir = $(PREFIX)/include

ifeq ($(origin BINDIR), undefined)
  bindir = $(PREFIX)/bin
else
  bindir = $(BINDIR)
endif

# rpm variables

CWP = $(shell pwd)
BIN = $(shell basename $(CWP))

# Special modes

ifneq (,$(findstring debug,$(MAKECMDGOALS)))
  CFLAGS = $(CFLAGS_DEBUG)
  LDFLAGS = $(LDFLAGS_DEBUG)
endif

# Compilation directories

vpath %.cpp source
vpath %.h include
vpath %.o $(objdir)

# How to install

INSTALL_LIB = install -m 775
INSTALL_DATA = install -m 664

# The files to be compiled

SRCS = $(patsubst source/%,%,$(wildcard *.cpp source/*.cpp))
HDRS = $(patsubst include/%,%,$(wildcard *.h include/*.h))
OBJS = $(SRCS:%.cpp=%.o)

OBJFILES = $(OBJS:%.o=obj/%.o)

MAINSRCS = $(LIB:%=%.cpp)
SUBSRCS = $(filter-out $(MAINSRCS),$(SRCS))
SUBOBJS = $(SUBSRCS:%.cpp=%.o)
SUBOBJFILES = $(SUBOBJS:%.o=obj/%.o)

# For make depend:

ALLSRCS = $(wildcard *.cpp source/*.cpp)

.PHONY: test rpm

rpmsourcedir = /tmp/$(shell whoami)/rpmbuild

# The rules

all: objdir $(LIB)
debug: objdir $(LIB)
release: objdir $(LIB)

$(LIB): $(OBJS)
	ar rcs $(LIBDIR)/lib$(LIB).a $(OBJFILES)
	$(CC) -o $(LIBDIR)/lib$(LIB).so $(LDFLAGS) $(LIBDIRS) $(LIBS) $(OBJFILES)

clean:
	rm -f $(LIBDIR)/*.so* $(LIBDIR)/*.a $(OBJFILES) *~ source/*~ include/*~

install:
	mkdir -p $(libdir)
	mkdir -p $(includedir)
	@list=`cd include && ls -1 *.h`; \
	for hdr in $$list; do \
	  $(INSTALL_DATA) include/$$hdr $(includedir)/$$hdr; \
	done

	$(INSTALL_LIB) lib/* $(libdir)
	
objdir:
	@mkdir -p $(objdir)
	@mkdir -p $(LIBDIR)

rpm:    clean
	mkdir -p $(rpmsourcedir) ; \
        if [ -e $(LIB).spec ]; then \
          tar --transform "s,^./,libfminc/,"  --exclude-vcs -cf $(rpmsourcedir)/lib$(LIB).tar . ; \
          gzip -f $(rpmsourcedir)/lib$(LIB).tar ; \
          rpmbuild -ta $(rpmsourcedir)/lib$(LIB).tar.gz ; \
          rm -f $(rpmsourcedir)/lib$(LIB).tar.gz ; \
        else \
          echo $(rpmerr); \
        fi;

.SUFFIXES: $(SUFFIXES) .cpp

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $(objdir)/$@ $<

-include Dependencies
