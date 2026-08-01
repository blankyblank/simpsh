.POSIX:

include config.mk

SRC = \
  alloc.c \
  arith.c \
  builtins.c \
  env.c \
  errmsg.c \
  exec.c \
  expand.c \
  glob.c \
  history.c \
  input.c \
  job.c \
  lex.c \
  lineio.c \
  main.c \
  opts.c \
  parse.c \
  path.c \
  pipe.c \
  printf.c \
  sig.c \
  simpsh.c \
  test.c \
  var.c

EXTRAS = \
  builtins/basename.c \
  builtins/cat.c \
  builtins/cut.c \
  builtins/dirname.c \
  builtins/expand.c \
  builtins/fold.c \
  builtins/head.c \
  builtins/readlink.c \
  builtins/realpath.c \
  builtins/sleep.c \
  builtins/tail.c \
  builtins/tee.c \
  builtins/uniq.c \
  builtins/wc.c

HDR = \
  alloc.h \
  arith.h \
  arg.h \
  builtins.h \
  config.h \
  env.h \
  errmsg.h \
  exec.h \
  expand.h \
  glob.h \
  histeditshm.h \
  history.h \
  input.h \
  job.h \
  lex.h \
  lineio.h \
  main.h \
  opts.h \
  parse.h \
  path.h \
  pipe.h \
  sig.h \
  simd.h \
  simpsh.h \
  utils.h \
  var.h

OBJDIR = obj
TARGET = simpsh

.PHONY: all clean test install uninstall analyze examine bench

all: $(TARGET)

obj:
	@mkdir -p obj obj/builtins

obj/root.stamp: obj $(SRC) $(HDR)
	@h=$$(ls -t $(HDR) | head -1); \
		for f in $(SRC); do \
			o=obj/$${f%.c}.o; \
			if [ "$$o" -nt "$$f" ] && [ "$$o" -nt "$$h" ]; then \
				continue; \
			fi; \
			printf '  %s %s\n' "$(CC)" "$$f"; \
			$(CC) $(CFLAGS) -c $$f -o $$o; \
		done
	@touch $@
obj/builtins.stamp: obj $(EXTRAS) $(HDR)
	@h=$$(ls -t $(HDR) | head -1); \
	for f in $(EXTRAS); do \
		o=obj/builtins/$${f##*/}; o=$${o%.c}.o; \
		if [ "$$o" -nt "$$f" ] && [ "$$o" -nt "$$h" ]; then \
			continue; \
		fi; \
		printf '  %s %s\n' "$(CC)" "$$f"; \
		$(CC) $(CFLAGS) -c $$f -o $$o; \
	 done
	@touch $@

$(TARGET): obj obj/root.stamp obj/builtins.stamp
	$(CC) -o $@ obj/*.o obj/builtins/*.o $(CFLAGS) $(LDFLAGS) $(LDLIBS)

install:
	rm -f $(BINDIR)/simpsh
	install -m 755 simpsh $(BINDIR)/simpsh
uninstall:
	rm -f $(BINDIR)/simpsh
clean:
	rm -f simpsh
	rm -rf $(OBJDIR)
analyze:
	scan-build --force-analyze-debug-code --use-cc=$(CC) -enable-checker core -enable-checker unix  -analyze-headers -o reports make clean all
examine:
	# gcc -O2 -g -fdump-tree-optimized $(SRC)
	gcc -O2 -g -fopt-info-all=report.txt $(SRC) $(EXTRAS)
test:
	cd tests && ./runtests.sh
bench:
	hyperfine --warmup 4 './simpsh profile/bench.sh'
