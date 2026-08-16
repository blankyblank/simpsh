## paths
PREFIX = /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

## compiler: gcc | clang
CC = clang

## build profile: release | debug | sanitize | valgrind | profile
BUILD = sanitize

## linking: dynamic | static
BUILD_LINK = dynamic

## libedit: 1 to enable, empty to disable
LIBEDIT = 1

## gcov: set to "--coverage -fno-lto" to enable
GCOV =

# default flags
BASE = --std=c23 -I. -Wall -Wextra -pedantic -pipe
