CC := gcc

CFLAGS 		   := -I. -Og -Wall -Wextra -pedantic -pipe -g3
SANITIZE_FLAGS := -fsanitize=address,leak,undefined -fno-omit-frame-pointer
# CFLAGS 		   := -I. -Os -Wall -Wextra -pedantic -pipe
# CFLAGS 		   := -I. -Og -Wall -Wextra -pedantic -pipe -g3
# SANITIZE_FLAGS := -static-libasan
# SANITIZE_FLAGS := -fsanitize=address,leak,undefined -fno-analyzer-state-merge
# SANITIZE_FLAGS := -fno-omit-frame-pointer

LDLIBS 	 	   := -lm /usr/lib64/libreadline.so

OBJDIR 	 	   := obj
SRC 	 	   := $(wildcard *.c)
OBJ 	 	   := $(patsubst %.c, $(OBJDIR)/%.o, $(SRC))

TARGET		   := simpsh
CFLAGS		   := $(CFLAGS) $(SANITIZE_FLAGS)

.PHONY: all clean install uninstall

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
