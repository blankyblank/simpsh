CC = gcc

CFLAGS = -I. -march=native -O -Wall -Wextra -pedantic -pipe
LDLIBS = -lm /usr/lib/libreadline.so
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
	rm -f simpsh *.o

install:
	rm -f /home/blank/.local/bin/simpsh
	cp -f simpsh /home/blank/.local/bin/
	chmod 755 /home/blank/.local/bin/simpsh

uninstall:
	rm -f /home/blank/.local/bin/simpsh
