CC := gcc
# CC := clang

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
  LDFLAGS += -ggdb -fvar-tracking-assignments -fno-analyzer-state-merge
endif
ifeq ($(BUILD),sanitize)
  CFLAGS += -Og -g3
  ifeq ($(BUILD_LINK),static)
    CFLAGS += -fsanitize=undefined,bounds -fno-omit-frame-pointer
    LDFLAGS += -static-libasan -fsanitize=undefined,bounds
  else
    CFLAGS += -fsanitize=address,leak,undefined,bounds -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address,leak,undefined,bounds
  endif
endif
ifeq ($(BUILD),valgrind)
  CFLAGS += -Og -g3 -DDEBUG -DENABLE_VALGRIND
	# callgrind flags
  # CFLAGS += -g -O2 -DDEBUG -DENABLE_VALGRIND
endif
ifeq ($(BUILD),profile)
  CFLAGS += -Og -g3 -fvar-tracking-assignments -fno-analyzer-state-merge -pg
  LDFLAGS += -fvar-tracking-assignments -fno-analyzer-state-merge -pg
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
