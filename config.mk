## paths
PREFIX = /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

## compiler: cc | gcc | clang | afl-clang-fast
CC = cc

## build profile: release | debug | sanitize | sanitize-extra (clang only) | valgrind | profile
BUILD = release

## linking: dynamic | static
BUILD_LINK = dynamic

## libedit: 1 to enable, empty to disable
LIBEDIT = 1

## gcov: set to "--coverage -fno-lto" to enable
GCOV =

# default flags
BASE = --std=c23 -I. -Wall -Wextra -pedantic -pipe

# EXTRA = -O0
# EXTRA = -DDEBUG
