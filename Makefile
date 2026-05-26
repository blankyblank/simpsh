CC := gcc
# cc | gcc | clang
BUILD       ?= sanitize
# debug | release | sanitize | valgrind | profile
BUILD_LINK  ?= dynamic
# dynamic | static
READLINE    := y
# set to anything to enable, unset to disable

# Compiler flags
CFLAGS  := --std=c99 -I. -Wall -Wextra -pedantic -pipe
LDFLAGS :=
LDLIBS  :=

# Build mode presets
ifeq ($(BUILD),release)
	CFLAGS += -march=native -Os -flto
endif
ifeq ($(BUILD),debug)
	CFLAGS += -Og -g3 -ggdb -fvar-tracking-assignments -fno-analyzer-state-merge
  ifeq ($(CC),gcc)
  		CFLAGS += -ggdb
  		LDFLAGS += -ggdb
  	endif
  	ifeq ($(CC),clang)
  		CFLAGS += -glldb -fstandalone-debug
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
	# just use gcc for valgrind
	CC := gcc
	CFLAGS += -Og -g3 -DDEBUG -DENABLE_VALGRIND
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
ifeq ($(BUILD_LINK),static)
	LDFLAGS += -static
endif
# Readline
ifdef READLINE
	CFLAGS += -DREADLINE
	LDLIBS += -lreadline
  ifeq ($(BUILD_LINK),static)
		LDLIBS += -lncurses
  endif
endif

OBJDIR 	 	 := obj
SRC 	 	   := $(wildcard *.c)
OBJ 	 	   := $(patsubst %.c, $(OBJDIR)/%.o, $(SRC))

TARGET		   := simpsh
CFLAGS		   := $(CFLAGS)

.PHONY: all clean test install uninstall

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
	rm -f /home/blank/.local/bin/simpsh
	cp -f simpsh /home/blank/.local/bin/
	chmod 755 /home/blank/.local/bin/simpsh
uninstall:
	rm -f /home/blank/.local/bin/simpsh
clean:
	rm -f simpsh obj/*.o

test:
	cd tests && ./runtests.sh
