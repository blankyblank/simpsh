## cc | gcc | clang
CC := gcc

## paths
PREFIX := /usr/local
BINDIR := $(DESTDIR)$(PREFIX)/bin

## release | debug | sanitize | valgrind | profile | trace
BUILD       ?= release

## dynamic | static | static-musl
BUILD_LINK  ?= static

## libedit
LIBEDITFLAGS := -DLIBEDIT
ifeq ($(BUILD_LINK),static)
  LIBEDITLIBS := -ledit -lncurses
  LIBEDITFLAGS += -DSTATICLIBEDIT
else
  LDLIBS += -ldl
endif

## sanitizer flags: use with the sanitize build target
ASANFLAGS := -fsanitize=address,undefined
## clang extras: -fsanitize=implicit-conversion | -fsanitize=integer
ASANFLAGS += $(if $(filter clang,$(CC)),-fsanitize=integer)
# ASANFLAGS += -fsanitize=cfi -fvisibility=hidden -O2 -flto

## valgrind profiling i.e. callgrind/cachegrind
# PROFFLAGS := -DDEBUG -DENABLE_VALGRIND -fxray-instrument
PROFFLAGS := -fxray-instrument

## set to anything to enable, unset to disable
GCOV  :=

## Compiler flags
CFLAGS  := --std=c23 -I. -Wall -Wextra -pedantic -pipe $(LIBEDITFLAGS)
LDFLAGS := -Wl,-z,now
LDLIBS  := $(LIBEDITLIBS) $(LIBEDIT-STATICLIBS)

