CC := gcc

# normal debug build
CFLAGS 		     :=  --std=c99 -I. -Og -Wall -Wextra -pedantic -pipe
# gdb debugging flags.
GDBFLAGS 		     :=  -g3 -ggdb -fvar-tracking-assignments -fno-analyzer-state-merge
# release build
#CFLAGS 		   := --std=c99 -I. -Os -Wall -Wextra -pedantic -pipe
# adress sanatizer flags
# SANITIZE_FLAGS := -static-libasan
# SANITIZE_FLAGS := -fno-omit-frame-pointer
SANITIZE_FLAGS := -DDEBUG -DENABLE_VALGRIND
# SANITIZE_FLAGS := -fsanitize=address,leak,undefined,bounds
# SANITIZE_FLAGS := -fsanitize=address,leak,undefined,bounds -fno-omit-frame-pointer

LDLIBS 	 	   := -lm /usr/lib64/libreadline.so

OBJDIR 	 	   := obj
SRC 	 	   := $(wildcard *.c)
OBJ 	 	   := $(patsubst %.c, $(OBJDIR)/%.o, $(SRC))

TARGET		   := simpsh
CFLAGS		   := $(CFLAGS) $(GDBFLAGS) $(SANITIZE_FLAGS)

.PHONY: all clean test install uninstall

all: $(TARGET)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $@

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)

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
