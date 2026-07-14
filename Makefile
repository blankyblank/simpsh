.POSIX:

include config.mk

ifneq ($(filter debug valgrind sanitize,$(BUILD)),)
	DEBUGFLAGS := -g3 -fno-omit-frame-pointer
	CFLAGS += $(if $(filter gcc,$(CC)),-ggdb -fvar-tracking-assignments -fno-analyzer-state-merge)
	CFLAGS += $(if $(filter clang,$(CC)),-glldb -fstandalone-debug)
endif

ifeq ($(BUILD),release)
	CFLAGS += -march=native
	# CFLAGS += -march=native -O3 -ffast-math -flto
	CFLAGS += $(if $(filter gcc,$(CC)),-O2 -Wno-stringop-overflow -flto=auto)
	CFLAGS += $(if $(filter clang,$(CC)), -flto -O3 -flto=full)
else ifeq ($(BUILD),debug)
	CFLAGS += $(DEBUGFLAGS)
	CFLAGS += $(if $(filter gcc,$(CC)),-Og -flto=auto)
	CFLAGS += $(if $(filter clang,$(CC)), -flto -Og -glldb -fstandalone-debug -flto=full)
else ifeq ($(BUILD),valgrind)
	CC := gcc
	CFLAGS += -Og $(DEBUGFLAGS) -DDEBUG -DENABLE_VALGRIND
else ifeq ($(BUILD),profile)
	CFLAGS += -O2 -g3 $(PROFFLAGS)
	CFLAGS += $(if $(filter gcc,$(CC)),-fvar-tracking-assignments -fno-analyzer-state-merge -pg)
	LDFLAGS += $(if $(filter gcc,$(CC)),-pg)
	CFLAGS += $(if $(filter clang,$(CC)),-fprofile-instr-generate -fcoverage-mapping)
	LDFLAGS += $(if $(filter clang,$(CC)),-fprofile-instr-generate)
else ifeq ($(BUILD),sanitize)
	CFLAGS += -O1 $(DEBUGFLAGS)
  ifeq ($(BUILD_LINK),static)
		CFLAGS += -fsanitize=undefined,bounds
		LDFLAGS += -fsanitize=undefined,bounds
		LDFLAGS += $(if $(filter gcc,$(CC)),-static-libasan)
		CFLAGS += $(if $(filter clang,$(CC)),-glldb -fstandalone-debug)
		LDFLAGS += $(if $(filter clang,$(CC)),-static-libsan)
	else
		CFLAGS += $(ASANFLAGS)
		LDFLAGS += $(ASANFLAGS)
	endif
endif

ifdef GCOV
	CFLAGS += --coverage -fno-lto
	LDFLAGS += --coverage
endif

# Link type
LDFLAGS += $(if $(filter static static-musl,$(BUILD_LINK)),-static)
ifeq ($(BUILD_LINK),static-musl)
	CLANG_RESOURCE_DIR := $(shell clang -print-resource-dir)
	CFLAGS +=  -DMUSL
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
	scan-build --force-analyze-debug-code --use-cc=$(CC) -enable-checker core -enable-checker unix  -analyze-headers -o reports make clean all
examine:
	# gcc -O2 -g -fdump-tree-optimized $(SRC)
	gcc -O2 -g -fopt-info-all=report.txt $(SRC)
test:
	cd tests && ./runtests.sh
bench:
	hyperfine --warmup 4 './simpsh profiling/bench.sh'
