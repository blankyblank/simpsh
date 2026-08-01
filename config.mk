####### cc | gcc | clang
CC = gcc

####### paths
PREFIX = /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

####### build type. use one line.
### 			release
CFLAG = -march=native -fno-plt -O2 -flto=auto -s
LDFLAG = -flto=auto
### (clang)
# CFLAG = -march=native -fno-plt -flto -O2 -fvectorize -flto=full
# LDFLAG = -flto=full -Wl,--strip-all

######## debug
# CFLAG = -Og -g3 -fno-omit-frame-pointer -flto=auto -ggdb
# LDFLAG = -flto=auto
### (clang)
# CFLAG = -Og -g3 -fno-omit-frame-pointer -flto -glldb -fstandalone-debug
# LDFLAG =
### 			valgrind (CC must be gcc)
# CFLAG = -Og -g3 -fno-omit-frame-pointer -DDEBUG -DENABLE_VALGRIND
# LDFLAG =

######## sanitize
# CFLAG = -O1 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined
# LDFLAG = -fsanitize=address,undefined $(CLANGASANLDFLAGS)
### (clang)
#CLANGASANLDFLAGS = -static-libasan
## sanitizer flags: use with the sanitize build target
## clang extras: -fsanitize=implicit-conversion | -fsanitize=integer
# ASANFLAGS = -fsanitize=integer

#NOTE: check if xray-instrument works with gcc even

######### profile
# CFLAG = -O2 -g3 -pg -fxray-instrument -fvar-tracking-assignments -fno-analyzer-state-merge
# LDFLAG = -pg
### (clang)
# CFLAG = -O2 -g3 -fprofile-instr-generate -fcoverage-mapping -fxray-instrument
# LDFLAG = -fprofile-instr-generate

######## linker flags
###			dynamic + libedit
LIBEDITLIBS = -ldl
LIBEDITFLAGS = -DLIBEDIT
###			 static + libedit
# LIBEDITLIBS = -ledit -lncurses
# STATICLIBEDIT = -DSTATICLIBEDIT
###			static
# STATIC = -static


###			gcov
#GCOVFLAGS = --coverage -fno-lto

BASE   = --std=c23 -I. -Wall -Wextra -pedantic -pipe
## Compiler flags
CFLAGS  = $(BASE) $(STATICLIBEDIT) $(CFLAG) $(LIBEDITFLAGS) $(GCOVFLAGS)
LDFLAGS = -Wl,-z,now $(LDFLAG) $(STATIC) $(GCOVFLAGS)
LDLIBS  = $(LIBEDITLIBS)
