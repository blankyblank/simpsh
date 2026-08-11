## for portability reasons while not requiring a dependency, or external
## scripts this file is where you adjust the build type. you have to pick one of:
## CC, CFLAG, and LDFLAG.
## and comment the lines in the old build "profile".
## you can also chose, static or dynamic linking, and whether or not to include libedit

####### cc | gcc | clang
CC = gcc

####### paths
PREFIX = /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

####### build type. use one line.
### 			release
CFLAG = -march=native -falign-functions=16 -fno-plt -O2 -flto=auto -s
LDFLAG = -flto=auto
### (clang)
# CFLAG = -march=native -fno-plt -flto -O2 -fvectorize -flto=full
# LDFLAG = -flto=full -Wl,--strip-all

######## debug
# CFLAG = -Og -g3 -fno-omit-frame-pointer -flto=auto -ggdb $(TRACE)
# LDFLAG = -flto=auto
# TRACE = -DDEBUG
### (clang)
# CFLAG = -Og -g3 -fno-omit-frame-pointer -flto -glldb -fstandalone-debug
# LDFLAG =
### 			valgrind (CC must be gcc)
# CFLAG = -Og -g3 -fno-omit-frame-pointer -DENABLE_VALGRIND
# LDFLAG =

######## sanitize
# CFLAG = -O1 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined  $(CLANGASANFLAGS)
# LDFLAG = -fsanitize=address,undefined $(CLANGASANLDFLAGS)
### (clang)
# CLANGASANLDFLAGS = -static-libasan
## sanitizer flags: use with the sanitize build target
## clang extras: -fsanitize=implicit-conversion | -fsanitize=integer
# CLANGASANFLAGS = -fsanitize=integer

######### profile
# CFLAG = -O2 -g3 -pg -fxray-instrument -fvar-tracking-assignments -fno-analyzer-state-merge
# LDFLAG = -pg
### (clang)
# CFLAG = -O2 -g3 -fprofile-instr-generate -fcoverage-mapping -fxray-instrument
# LDFLAG = -fprofile-instr-generate

######## linker flags
###			dynamic + libedit (uncomment these two for dynamic)
LIBEDITLIBS = -ldl
LIBEDITFLAGS = -DLIBEDIT
###			 static + libedit (uncomment these tree for static)
# LIBEDITLIBS = -ledit -lncurses
# STATICLIBEDIT = -DSTATICLIBEDIT
###			static
# STATIC = -static


###			gcov
#GCOVFLAGS = --coverage -fno-lto

# gcc or clang
BASE   = --std=c23 -I. -Wall -Wextra -pedantic -pipe
# for tcc
# BASE   =  -std=c11 -I. -Wall -Wextra -pedantic -pipe

CFLAGS  = $(BASE) $(STATICLIBEDIT) $(CFLAG) $(LIBEDITFLAGS) $(GCOVFLAGS) $(PGOFLAGS)
LDFLAGS = -Wl,-z,now $(LDFLAG) $(STATIC) $(GCOVFLAGS) $(PGOFLAGS)
LDLIBS  = $(LIBEDITLIBS)
