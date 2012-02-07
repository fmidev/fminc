LIB = fminc

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

CFLAGS = -DUNIX -O2 -DNDEBUG $(MAINFLAGS) 
LDFLAGS = -s

# Special modes

CFLAGS_DEBUG = -DUNIX -O0 -g -DDEBUG $(MAINFLAGS) $(EXTRAFLAGS)
CFLAGS_PROFILE = -DUNIX -O2 -g -pg -DNDEBUG $(MAINFLAGS)

LDFLAGS_DEBUG =
LDFLAGS_PROFILE =

INCLUDES = -I include \
           -I$(includedir) \
           -I$(includedir)/netcdf-3

LIBS =  -L$(libdir) \
	-lboost_date_time \
        -lboost_program_options \
        -lboost_filesystem \
        -lboost_system \
        -lnetcdf_c++

# Common library compiling template

# Installation directories

processor := $(shell uname -p)

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

objdir = obj
libdir = lib

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

ifneq (,$(findstring profile,$(MAKECMDGOALS)))
  CFLAGS = $(CFLAGS_PROFILE)
  LDFLAGS = $(LDFLAGS_PROFILE)
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

# The rules

all: objdir $(LIB)
debug: objdir $(LIB)
release: objdir $(LIB)
profile: objdir $(LIB)

$(LIB): $(OBJS)
	ar rcs $(libdir)/lib$(LIB).a $(OBJFILES)

clean:
	rm -f $(libdir)/*.so $(libdir)/*.a $(OBJFILES) *~ source/*~ include/*~

install:
	mkdir -p $(bindir)
	@list='$(LIB)'; \
	for prog in $$list; do \
	  echo $(INSTALL_LIB) $$prog $(bindir)/$$prog; \
	  $(INSTALL_LIB) $$prog $(bindir)/$$prog; \
	done

depend:
	gccmakedep -fDependencies -- $(CFLAGS) $(INCLUDES) -- $(ALLSRCS)

objdir:
	@mkdir -p $(objdir)
	@mkdir -p $(libdir)


.SUFFIXES: $(SUFFIXES) .cpp

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $(objdir)/$@ $<

-include Dependencies
