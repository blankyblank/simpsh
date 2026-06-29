.POSIX:

include config.mk

ifeq ($(BUILD),release)
	CFLAGS += -march=native -O2 -flto=auto
# -D_FORTIFY_SOURCE=3
endif
ifdef GCOV
	CFLAGS += --coverage -fno-lto
	LDFLAGS += --coverage
endif
ifdef TRACE
	CFLAGS += -DTRACE
endif
ifeq ($(BUILD),debug)
	CFLAGS += -D_FORTIFY_SOURCE=3 -Og -g3 -fno-omit-frame-pointer
  ifeq ($(CC),gcc)
  		CFLAGS +=  -ggdb -fvar-tracking-assignments -fno-analyzer-state-merge
  		LDFLAGS += -ggdb
  	endif
  	ifeq ($(CC),clang)
  		CFLAGS += -glldb  -fstandalone-debug
  endif
endif
ifeq ($(BUILD),sanitize)
	CFLAGS += -O0 -g3  -fno-omit-frame-pointer
  ifeq ($(BUILD_LINK),static)
		CFLAGS += -fsanitize=undefined,bounds
		LDFLAGS += -fsanitize=undefined,bounds
    ifeq ($(CC),gcc)
    		CFLAGS += -ggdb
    		LDFLAGS += -static-libasan
    	endif
    	ifeq ($(CC),clang)
    		CFLAGS += -glldb -fstandalone-debug
				LDFLAGS += -static-libsan
    endif
  else
		CFLAGS += -fsanitize=address,leak,undefined,bounds
		LDFLAGS += -fsanitize=address,leak,undefined,bounds
    ifeq ($(CC),gcc)
    		CFLAGS += -ggdb
    		# LDFLAGS += -ggdb
    	endif
    	ifeq ($(CC),clang)
    		CFLAGS += -glldb -fstandalone-debug
    endif
endif
endif
ifeq ($(BUILD),valgrind)
	CC := gcc
	CFLAGS += -Og -g3 -ggdb -fno-omit-frame-pointer -DDEBUG -DENABLE_VALGRIND
	# callgrind flags
  # CFLAGS += -g -O2 -DDEBUG -DENABLE_VALGRIND
endif
ifeq ($(BUILD),profile)
	CFLAGS += -O2 -g3 -fvar-tracking-assignments -fno-analyzer-state-merge
  ifeq ($(CC),gcc)
  	CFLAGS += -pg
  	LDFLAGS += -pg
  endif
  ifeq ($(CC),clang)
		CFLAGS += -fprofile-instr-generate -fcoverage-mapping
		LDFLAGS += -fprofile-instr-generate
  endif
endif
# Link type
ifeq ($(BUILD_LINK),static-musl)
	CLANG_RESOURCE_DIR := $(shell clang -print-resource-dir)
	CFLAGS +=  -DMUSL
	LDFLAGS += -static
endif
ifeq ($(BUILD_LINK),static)
	LDFLAGS += -static
endif

OBJDIR 	 	 := obj
SRC 	 	   := $(wildcard *.c)
OBJ 	 	   := $(patsubst %.c, $(OBJDIR)/%.o, $(SRC))

TARGET		   := simpsh
CFLAGS		   := $(CFLAGS)

.PHONY: all clean test install uninstall analyze examine bench

all: $(TARGET)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR):
	mkdir -p $@
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

all.c:
	echo "int main() { return 0;}" > all.c

install:
	rm -f $(BINDIR)/simpsh
	install -m 755 simpsh $(BINDIR)/simpsh
uninstall:
	rm -f $(BINDIR)/simpsh
clean:
	rm -f simpsh obj/*.o
analyze:
	scan-build --use-cc=$(CC) -enable-checker core -enable-checker unix  -analyze-headers -o reports make clean all
examine:
	# gcc -O2 -g -fdump-tree-optimized $(SRC)
	gcc -O2 -g -fopt-info-all=report.txt $(SRC)
test:
	cd tests && ./runtests.sh
bench:
	hyperfine --warmup 4 './simpsh tests/bench.sh'
