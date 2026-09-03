.POSIX:

include config.mk

CCNAME != \
	if [ "$(CC)" = "cc" ]; then \
		p="$$(command -v $(CC) 2>/dev/null)"; \
		r="$$(readlink "$$p" 2>/dev/null)";\
		if [ -n "$$r" ]; then\
			case "$$r" in\
				*clang*) echo clang;;\
				*gcc*) echo gcc;;\
				*) echo other;;\
			esac;\
		else\
			v="$$($(CC) --version 2>/dev/null)";\
				case "$$v" in\
					*clang*) echo clang;;\
					*gcc*) echo gcc;;\
					*) echo other;;\
				esac;\
		fi \
	else \
		echo $(CC); \
	fi

OS != uname -s

PROFILE != case "$(BUILD):$(CCNAME):$(OS)" in \
	release:gcc:*)    echo "-march=native -falign-functions=16 -fno-plt -O2 -flto=auto -s" ;; \
	release:clang:*)  echo "-march=native -fno-plt -flto -O2 -fvectorize -flto=full" ;; \
	debug:gcc:*)      echo "-Og -g3 -fno-omit-frame-pointer -flto=auto -ggdb" ;; \
	debug:clang:*)    echo "-Og -g3 -fno-omit-frame-pointer -flto -glldb -fstandalone-debug" ;; \
	sanitize:gcc:OpenBSD)   echo "-O1 -g3 -fno-omit-frame-pointer -fsanitize=undefined" ;; \
	sanitize:gcc:*)   echo "-O1 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined" ;; \
	sanitize:clang:OpenBSD) echo "-O1 -g3 -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=integer" ;; \
	sanitize:clang:*) echo "-O1 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined -fsanitize=integer" ;; \
	valgrind:*)       echo "-Og -g3 -fno-omit-frame-pointer -DENABLE_VALGRIND" ;; \
	profile:gcc:*)    echo "-O2 -g3 -pg -fxray-instrument -fvar-tracking-assignments -fno-analyzer-state-merge" ;; \
	profile:clang:*)  echo "-O2 -g3 -fprofile-instr-generate -fcoverage-mapping -fxray-instrument" ;; \
	*)                echo "-march=native -O2 -flto=auto";;\
	esac

LDFLAG != case "$(BUILD):$(CCNAME):$(OS)" in \
	release:gcc:*)    echo "-flto=auto" ;; \
	release:clang:*)  echo "-flto=full -Wl,--strip-all" ;; \
	debug:gcc:*)      echo "-flto=auto" ;; \
	debug:clang:*)    echo "" ;; \
	sanitize:gcc:OpenBSD)   echo "-fsanitize=undefined" ;; \
	sanitize:gcc:*)   echo "-fsanitize=address,undefined" ;; \
	sanitize:clang:OpenBSD) echo "-fsanitize=undefined -static-libasan" ;; \
	sanitize:clang:*) echo "-fsanitize=address,undefined -static-libasan" ;; \
	valgrind:*)       echo "" ;; \
	profile:gcc:*)    echo "-pg" ;; \
	profile:clang:*)  echo "-fprofile-instr-generate" ;; \
	*)                echo "" ;; \
	esac

LIBEDITLIBS != case "$(BUILD_LINK):$(LIBEDIT):$(OS)" in\
	static:1:OpenBSD) echo "-ledit -lcurses" ;;\
	static:1:*) echo "-ledit -lncurses" ;;\
	*:1:OpenBSD) echo "" ;;\
	*:1:*) echo "-ldl" ;;\
	*) echo "" ;;\
	esac
LINK != case "$(BUILD_LINK)" in static) echo "-static" ;; esac
LIBEDITFLAGS != case "$(BUILD_LINK):$(LIBEDIT)" in "static:1") echo "-DLIBEDIT -DSTATICLIBEDIT" ;; *:1) echo "-DLIBEDIT" ;; *) echo "" ;; esac

CFLAGS := $(BASE) $(PROFILE) $(LIBEDITFLAGS)
CFLAGS += $(GCOV)
CFLAGS += $(PGOFLAGS)
CFLAGS += $(EXTRA)
LDFLAGS := -Wl,-z,now $(LDFLAG) $(LINK) $(GCOV) $(PGOFLAGS)
LDLIBS  := $(LIBEDITLIBS)

SRC = alloc.c arith.c builtins.c env.c errmsg.c exec.c expand.c glob.c history.c input.c job.c lex.c lineio.c main.c opts.c parse.c path.c pipe.c printf.c sig.c simpsh.c test.c var.c
EXTRAS = builtins/basename.c builtins/cat.c builtins/comm.c builtins/cut.c builtins/dirname.c builtins/expand.c builtins/fold.c builtins/head.c builtins/paste.c builtins/readlink.c builtins/realpath.c builtins/sleep.c builtins/sort.c builtins/tail.c builtins/tee.c builtins/tr.c builtins/uniq.c builtins/wc.c
HDR = alloc.h arg.h arith.h builtins.h config.h env.h errmsg.h exec.h expand.h glob.h histeditshm.h history.h input.h job.h lex.h lineio.h main.h opts.h parse.h path.h pipe.h sig.h simd.h simpsh.h utils.h var.h
OBJS = build/alloc.o build/arith.o build/builtins.o build/env.o build/errmsg.o build/exec.o build/expand.o build/glob.o build/history.o build/input.o build/job.o build/lex.o build/lineio.o build/main.o build/opts.o build/parse.o build/path.o build/pipe.o build/printf.o build/sig.o build/simpsh.o build/test.o build/var.o build/builtins/basename.o build/builtins/cat.o build/builtins/comm.o build/builtins/cut.o build/builtins/dirname.o build/builtins/expand.o build/builtins/fold.o build/builtins/head.o build/builtins/paste.o build/builtins/readlink.o build/builtins/realpath.o build/builtins/sleep.o build/builtins/sort.o build/builtins/tail.o build/builtins/tee.o build/builtins/tr.o build/builtins/uniq.o build/builtins/wc.o

.OBJDIR: .
OBJDIR = build
TARGET = simpsh

.PHONY: all clean pgo test install uninstall analyze examine bench bench-a bench-e bench-f bench-p bench-q

all: $(TARGET)
build/builtins:
	@mkdir -p build/builtins
build/alloc.o: alloc.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c alloc.c -o $@
build/arith.o: arith.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c arith.c -o $@
build/builtins.o: builtins.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c builtins.c -o $@
build/env.o: env.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c env.c -o $@
build/errmsg.o: errmsg.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c errmsg.c -o $@
build/exec.o: exec.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c exec.c -o $@
build/expand.o: expand.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c expand.c -o $@
build/glob.o: glob.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c glob.c -o $@
build/history.o: history.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c history.c -o $@
build/input.o: input.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c input.c -o $@
build/job.o: job.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c job.c -o $@
build/lex.o: lex.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c lex.c -o $@
build/lineio.o: lineio.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c lineio.c -o $@
build/main.o: main.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c main.c -o $@
build/opts.o: opts.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c opts.c -o $@
build/parse.o: parse.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c parse.c -o $@
build/path.o: path.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c path.c -o $@
build/pipe.o: pipe.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c pipe.c -o $@
build/printf.o: printf.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c printf.c -o $@
build/sig.o: sig.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c sig.c -o $@
build/simpsh.o: simpsh.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c simpsh.c -o $@
build/test.o: test.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c test.c -o $@
build/var.o: var.c $(HDR) build/builtins
	$(CC) $(CFLAGS) -c var.c -o $@
build/builtins/basename.o: builtins/basename.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_basename 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/basename.c "$@"; $(CC) $(CFLAGS) -c builtins/basename.c -o $@; else rm -f $@; fi
build/builtins/cat.o: builtins/cat.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_cat 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/cat.c "$@"; $(CC) $(CFLAGS) -c builtins/cat.c -o $@; else rm -f $@; fi
build/builtins/comm.o: builtins/comm.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_comm 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/comm.c "$@"; $(CC) $(CFLAGS) -c builtins/comm.c -o $@; else rm -f $@; fi
build/builtins/cut.o: builtins/cut.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_cut 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/cut.c "$@"; $(CC) $(CFLAGS) -c builtins/cut.c -o $@; else rm -f $@; fi
build/builtins/dirname.o: builtins/dirname.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_dirname 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/dirname.c "$@"; $(CC) $(CFLAGS) -c builtins/dirname.c -o $@; else rm -f $@; fi
build/builtins/expand.o: builtins/expand.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_expand 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/expand.c "$@"; $(CC) $(CFLAGS) -c builtins/expand.c -o $@; else rm -f $@; fi
build/builtins/fold.o: builtins/fold.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_fold 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/fold.c "$@"; $(CC) $(CFLAGS) -c builtins/fold.c -o $@; else rm -f $@; fi
build/builtins/head.o: builtins/head.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_head 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/head.c "$@"; $(CC) $(CFLAGS) -c builtins/head.c -o $@; else rm -f $@; fi
build/builtins/paste.o: builtins/paste.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_paste 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/paste.c "$@"; $(CC) $(CFLAGS) -c builtins/paste.c -o $@; else rm -f $@; fi
build/builtins/readlink.o: builtins/readlink.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_readlink 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/readlink.c "$@"; $(CC) $(CFLAGS) -c builtins/readlink.c -o $@; else rm -f $@; fi
build/builtins/realpath.o: builtins/realpath.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_realpath 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/realpath.c "$@"; $(CC) $(CFLAGS) -c builtins/realpath.c -o $@; else rm -f $@; fi
build/builtins/sleep.o: builtins/sleep.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_sleep 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/sleep.c "$@"; $(CC) $(CFLAGS) -c builtins/sleep.c -o $@; else rm -f $@; fi
build/builtins/sort.o: builtins/sort.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_sort 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/sort.c "$@"; $(CC) $(CFLAGS) -c builtins/sort.c -o $@; else rm -f $@; fi
build/builtins/tail.o: builtins/tail.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_tail 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/tail.c "$@"; $(CC) $(CFLAGS) -c builtins/tail.c -o $@; else rm -f $@; fi
build/builtins/tee.o: builtins/tee.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_tee 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/tee.c "$@"; $(CC) $(CFLAGS) -c builtins/tee.c -o $@; else rm -f $@; fi
build/builtins/tr.o: builtins/tr.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_tr 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/tr.c "$@"; $(CC) $(CFLAGS) -c builtins/tr.c -o $@; else rm -f $@; fi
build/builtins/uniq.o: builtins/uniq.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_uniq 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/uniq.c "$@"; $(CC) $(CFLAGS) -c builtins/uniq.c -o $@; else rm -f $@; fi
build/builtins/wc.o: builtins/wc.c $(HDR) build/builtins
	@if grep -qi "^#define ENABLE_wc 1" config.h; then echo "$(CC)" "$(CFLAGS)" -c builtins/wc.c "$@"; $(CC) $(CFLAGS) -c builtins/wc.c -o $@; else rm -f $@; fi

$(TARGET): $(OBJS)
	@echo "$(CC) $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)"
	@$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS) $(LDLIBS)

install:
	rm -f $(BINDIR)/simpsh
	install -m 755 simpsh $(BINDIR)/simpsh

uninstall:
	rm -f $(BINDIR)/simpsh

clean:
	rm -f simpsh
	rm -rf $(OBJDIR)

pgo:
	rm -rf pgo
	$(MAKE) clean
	$(MAKE) PGOFLAGS="-fprofile-dir=pgo -fprofile-generate"
	./simpsh profile/bench.sh
	./simpsh ./profile/parse.bench
	./simpsh ./profile/forbench.sh
	./simpsh ./profile/quote-bench.sh
	./simpsh ./profile/arith-bench.sh
	./simpsh ./profile/printf-bench.sh
	./simpsh ./profile/benchwecho.sh
	cd tests && ./runtests.sh > /dev/null 2>&1 || true
	$(MAKE) clean
	$(MAKE) PGOFLAGS="-fprofile-dir=pgo -fprofile-use -fprofile-correction"

analyze:
	scan-build --force-analyze-debug-code --use-cc=$(CC) -enable-checker core -enable-checker unix  -analyze-headers -o reports make clean all

examine:
	# gcc -O2 -g -fdump-tree-optimized $(SRC)
	gcc -O2 -g -fopt-info-all=report.txt $(SRC) $(EXTRAS)

test:
	cd tests && ./runtests.sh

bench:
	hyperfine --warmup 4 'simpsh profile/bench.sh'
bench-a:
	hyperfine --warmup 4 'simpsh ./profile/arith-bench.sh'
bench-e:
	hyperfine --warmup 4 'simpsh ./profile/benchwecho.sh'
bench-f:
	hyperfine --warmup 4 'simpsh ./profile/forbench.sh'
bench-p:
	hyperfine --warmup 4 'simpsh ./profile/printf-bench.sh'
bench-q:
	hyperfine --warmup 4 'simpsh ./profile/quote-bench.sh'
