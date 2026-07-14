## cc | gcc | clang
CC := clang

## paths
PREFIX := /usr/local
BINDIR := $(DESTDIR)$(PREFIX)/bin

## release | debug | sanitize | valgrind | profile | trace
BUILD       ?= release

## dynamic | static | static-musl
BUILD_LINK  ?= dynamic

## libedit
LIBEDITFLAGS := -DLIBEDIT
ifeq ($(BUILD_LINK),static)
  LIBEDITLIBS := -ledit -lncurses
  CFLAGS += -DSTATICLIBEDIT
  LDLIBS += $(LIBEDITLIBS)
else
  LDLIBS += -ldl
endif

## sanitizer flags: use with the sanitize build target
ASANFLAGS := -fsanitize=address,undefined
## clang extras: -fsanitize=implicit-conversion | -fsanitize=integer
ASANFLAGS += -fsanitize=integer
# ASANFLAGS += -fsanitize=cfi -fvisibility=hidden -O2 -flto
# export UBSAN_OPTIONS=print_stacktrace=1:abort_on_error=1

## valgrind profiling i.e. callgrind/cachegrind
# PROFFLAGS := -DDEBUG -DENABLE_VALGRIND -fxray-instrument
PROFFLAGS := -fxray-instrument

## set to anything to enable, unset to disable
GCOV  :=

## Compiler flags
CFLAGS  := --std=c99 -I. -Wall -Wextra -pedantic -pipe -fno-plt $(LIBEDITFLAGS)
LDFLAGS := -Wl,-z,now
LDLIBS  := $(LIBEDITLIBS) $(LIBEDIT-STATICLIBS)

