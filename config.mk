## paths
PREFIX = /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

## compiler: gcc | clang
CC = gcc

## build profile: release | debug | sanitize | valgrind | profile
BUILD = release

## linking: dynamic | static
BUILD_LINK = dynamic

## libedit: 1 to enable, empty to disable
LIBEDIT = 1

## gcov: set to "--coverage -fno-lto" to enable
GCOV =

# default flags
BASE = --std=c23 -I. -Wall -Wextra -pedantic -pipe

# EXTRA = -pg
# EXTRA = -DDEBUG
