## cc | gcc | clang
CC := gcc

## paths
PREFIX := /usr/local
BINDIR := $(DESTDIR)$(PREFIX)/bin

## release | debug | sanitize | valgrind | profile | trace
BUILD       ?= release

## dynamic | static | static-musl
BUILD_LINK  ?= dynamic

## libedit
LIBEDITFLAGS := -DLIBEDIT

## sanitizer flags: use with the sanitize build target
ASANFLAGS := -fsanitize=address,undefined
## clang extras: -fsanitize=implicit-conversion | -fsanitize=integer
ASANFLAGS += $(if $(filter clang,$(CC)),-fsanitize=integer)

## valgrind profiling i.e. callgrind/cachegrind
# PROFFLAGS := -DDEBUG -DENABLE_VALGRIND -fxray-instrument
PROFFLAGS := -fxray-instrument

## set to anything to enable, unset to disable
GCOV  :=

## Compiler flags
CFLAGS  := --std=c23 -I. -Wall -Wextra -pedantic -pipe $(LIBEDITFLAGS)
LDFLAGS := -Wl,-z,now
LDLIBS  := $(LIBEDITLIBS)

## extra builtins
EXTRAS := \
	basename \
	cat \
	cut \
	dirname \
	expand \
	fold \
	head \
	readlink \
	realpath \
	sleep \
	tail \
	tee \
	wc

