## cc | gcc | clang
CC := clang

## paths
PREFIX := /usr/local
BINDIR := $(DESTDIR)$(PREFIX)/bin

## release | debug | sanitize | valgrind | profile | trace
BUILD       ?= sanitize

## dynamic | static | static-musl
BUILD_LINK  ?= dynamic

## libedit
LIBEDITFLAGS := -DLIBEDIT
LIBEDITLIBS := -ledit
## uncomment for the static build target with libedit
# LIBEDIT-STATICLIBS := -lhistory -lncurses

## set to anything to enable, unset to disable
GCOV  :=
TRACE :=

## Compiler flags
CFLAGS  := --std=c99 -I. -Wall -Wextra -pedantic -pipe $(LIBEDITFLAGS)
LDFLAGS :=
LDLIBS  := $(LIBEDITLIBS) $(LIBEDIT-STATICLIBS)

# export UBSAN_OPTIONS=print_stacktrace=1:abort_on_error=1

## sanitizer flags: use with the sanitize build target
ASANFLAGS := -fsanitize=address,undefined
## clang extras
# ASANFLAGS += -fsanitize=integer
# ASANFLAGS += -fsanitize=implicit-conversion
# ASANFLAGS += -fsanitize=cfi -fvisibility=hidden -O2 -flto

## valgrind profiling i.e. callgrind/cachegrind
PROFFLAGS := -DDEBUG -DENABLE_VALGRIND


