# cc | gcc | clang
CC := gcc

#paths
PREFIX := /usr/local
BINDIR := $(DESTDIR)$(PREFIX)/bin

# release | debug | sanitize | valgrind | profile | trace
BUILD       ?= release

# dynamic | static | static-musl
BUILD_LINK  ?= dynamic

# libedit
LIBEDITFLAGS := -DLIBEDIT
LIBEDITLIBS := -ledit
# uncomment for the static build target with libedit
# LIBEDIT-STATICLIBS := -lhistory -lncurses

# set to anything to enable, unset to disable
GCOV  :=
TRACE :=

# Compiler flags
CFLAGS  := --std=c99 -I. -Wall -Wextra -pedantic -pipe
LDFLAGS := $(LIBEDITFLAGS)
LDLIBS  := $(LIBEDITLIBS) $(LIBEDIT-STATICLIBS)
