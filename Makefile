CC = gcc

# CFLAGS = -I. -Os -Wall -Wextra -pedantic -pipe
#CFLAGS = -I. -O0 -Wall -Wextra -pedantic -pipe -g -static-libasan
# CFLAGS = -I. -Og -Wall -Wextra -pedantic -pipe -g -fsanitize=address,leak,undefined -fno-analyzer-state-merge
CFLAGS = -I. -Og -Wall -Wextra -pedantic -pipe -g3 -fsanitize=address,leak,undefined -fno-omit-frame-pointer
# CFLAGS = -I. -Og -Wall -Wextra -pedantic -pipe -g3 -fno-omit-frame-pointer
LDLIBS = -lm /usr/lib64/libreadline.so
OBJDIR = obj
SRC 	 = $(wildcard *.c)
#)_OBJ 	 = simpsh.o builtins.o exec.o

OBJ 	 = $(patsubst %.c, $(OBJDIR)/%.o, $(SRC))

simpsh: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)
all.c:
	echo "int main() { return 0;}" > all.c

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
.PHONY: clean
clean:
	rm -f simpsh obj/*.o

install:
	rm -f /home/blank/.local/bin/simpsh
	cp -f simpsh /home/blank/.local/bin/
	chmod 755 /home/blank/.local/bin/simpsh

uninstall:
	rm -f /home/blank/.local/bin/simpsh
