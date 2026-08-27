.POSIX:

include config.mk


PROFILE != case "$(BUILD):$(CC)" in \
	"release:gcc")    echo "-march=native -falign-functions=16 -fno-plt -O2 -flto=auto -s" ;; \
	"release:clang")  echo "-march=native -fno-plt -flto -O2 -fvectorize -flto=full" ;; \
	"debug:gcc")      echo "-Og -g3 -fno-omit-frame-pointer -flto=auto -ggdb" ;; \
	"debug:clang")    echo "-Og -g3 -fno-omit-frame-pointer -flto -glldb -fstandalone-debug" ;; \
	"sanitize:gcc")   echo "-O1 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined" ;; \
	"sanitize:clang") echo "-O1 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined -fsanitize=integer" ;; \
	valgrind:*)       echo "-Og -g3 -fno-omit-frame-pointer -DENABLE_VALGRIND" ;; \
	"profile:gcc")    echo "-O2 -g3 -pg -fxray-instrument -fvar-tracking-assignments -fno-analyzer-state-merge" ;; \
	"profile:clang")  echo "-O2 -g3 -fprofile-instr-generate -fcoverage-mapping -fxray-instrument" ;; \
	*)                echo "error: unknown BUILD/CC ($(BUILD)/$(CC))" >&2 ;; \
	esac

LDFLAG != case "$(BUILD):$(CC)" in \
	"release:gcc")    echo "-flto=auto" ;; \
	"release:clang")  echo "-flto=full -Wl,--strip-all" ;; \
	"debug:gcc")      echo "-flto=auto" ;; \
	"debug:clang")    echo "" ;; \
	"sanitize:gcc")   echo "-fsanitize=address,undefined" ;; \
	"sanitize:clang") echo "-fsanitize=address,undefined -static-libasan" ;; \
	valgrind:*)       echo "" ;; \
	"profile:gcc")    echo "-pg" ;; \
	"profile:clang")  echo "-fprofile-instr-generate" ;; \
	*)                echo "" ;; \
	esac

LINK != case "$(BUILD_LINK)" in static) echo "-static" ;; esac
LIBEDITFLAGS != case "$(BUILD_LINK):$(LIBEDIT)" in "static:1") echo "-DLIBEDIT -DSTATICLIBEDIT" ;; *:1) echo "-DLIBEDIT" ;; *) echo "" ;; esac
LIBEDITLIBS != case "$(BUILD_LINK):$(LIBEDIT)" in "static:1") echo "-ledit -lncurses" ;; *:1) echo "-ldl" ;; *) echo "" ;; esac

CFLAGS := $(BASE) $(PROFILE) $(LIBEDITFLAGS)
CFLAGS += $(GCOV)
CFLAGS += $(PGOFLAGS)
CFLAGS += $(EXTRA)
LDFLAGS := -Wl,-z,now $(LDFLAG) $(LINK) $(GCOV) $(PGOFLAGS)
LDLIBS  := $(LIBEDITLIBS)

SRC = alloc.c arith.c builtins.c env.c errmsg.c exec.c expand.c glob.c history.c input.c job.c lex.c lineio.c main.c opts.c parse.c path.c pipe.c printf.c sig.c simpsh.c test.c var.c
EXTRAS = builtins/basename.c builtins/cat.c builtins/comm.c builtins/cut.c builtins/dirname.c builtins/expand.c builtins/fold.c builtins/head.c builtins/paste.c builtins/readlink.c builtins/realpath.c builtins/sleep.c builtins/sort.c builtins/tail.c builtins/tee.c builtins/tr.c builtins/uniq.c builtins/wc.c
HDR = alloc.h arg.h arith.h builtins.h config.h env.h errmsg.h exec.h expand.h glob.h histeditshm.h history.h input.h job.h lex.h lineio.h main.h opts.h parse.h path.h pipe.h sig.h simd.h simpsh.h utils.h var.h
OBJS = obj/alloc.o obj/arith.o obj/builtins.o obj/env.o obj/errmsg.o obj/exec.o obj/expand.o obj/glob.o obj/history.o obj/input.o obj/job.o obj/lex.o obj/lineio.o obj/main.o obj/opts.o obj/parse.o obj/path.o obj/pipe.o obj/printf.o obj/sig.o obj/simpsh.o obj/test.o obj/var.o obj/builtins/basename.o obj/builtins/cat.o obj/builtins/comm.o obj/builtins/cut.o obj/builtins/dirname.o obj/builtins/expand.o obj/builtins/fold.o obj/builtins/head.o obj/builtins/paste.o obj/builtins/readlink.o obj/builtins/realpath.o obj/builtins/sleep.o obj/builtins/sort.o obj/builtins/tail.o obj/builtins/tee.o obj/builtins/tr.o obj/builtins/uniq.o obj/builtins/wc.o

.OBJDIR: .
OBJDIR = obj
TARGET = simpsh

.PHONY: all clean pgo test install uninstall analyze examine bench bench-a bench-e bench-f bench-p bench-q

all: $(TARGET)
obj/builtins:
	@mkdir -p obj/builtins
obj/alloc.o: alloc.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c alloc.c -o $@
obj/arith.o: arith.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c arith.c -o $@
obj/builtins.o: builtins.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c builtins.c -o $@
obj/env.o: env.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c env.c -o $@
obj/errmsg.o: errmsg.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c errmsg.c -o $@
obj/exec.o: exec.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c exec.c -o $@
obj/expand.o: expand.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c expand.c -o $@
obj/glob.o: glob.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c glob.c -o $@
obj/history.o: history.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c history.c -o $@
obj/input.o: input.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c input.c -o $@
obj/job.o: job.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c job.c -o $@
obj/lex.o: lex.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c lex.c -o $@
obj/lineio.o: lineio.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c lineio.c -o $@
obj/main.o: main.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c main.c -o $@
obj/opts.o: opts.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c opts.c -o $@
obj/parse.o: parse.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c parse.c -o $@
obj/path.o: path.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c path.c -o $@
obj/pipe.o: pipe.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c pipe.c -o $@
obj/printf.o: printf.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c printf.c -o $@
obj/sig.o: sig.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c sig.c -o $@
obj/simpsh.o: simpsh.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c simpsh.c -o $@
obj/test.o: test.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c test.c -o $@
obj/var.o: var.c $(HDR) obj/builtins
	$(CC) $(CFLAGS) -c var.c -o $@
obj/builtins/basename.o: builtins/basename.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_basename 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/basename.c -o $@; else rm -f $@; fi
obj/builtins/cat.o: builtins/cat.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_cat 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/cat.c -o $@; else rm -f $@; fi
obj/builtins/comm.o: builtins/comm.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_comm 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/comm.c -o $@; else rm -f $@; fi
obj/builtins/cut.o: builtins/cut.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_cut 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/cut.c -o $@; else rm -f $@; fi
obj/builtins/dirname.o: builtins/dirname.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_dirname 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/dirname.c -o $@; else rm -f $@; fi
obj/builtins/expand.o: builtins/expand.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_expand 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/expand.c -o $@; else rm -f $@; fi
obj/builtins/fold.o: builtins/fold.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_fold 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/fold.c -o $@; else rm -f $@; fi
obj/builtins/head.o: builtins/head.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_head 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/head.c -o $@; else rm -f $@; fi
obj/builtins/paste.o: builtins/paste.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_paste 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/paste.c -o $@; else rm -f $@; fi
obj/builtins/readlink.o: builtins/readlink.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_readlink 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/readlink.c -o $@; else rm -f $@; fi
obj/builtins/realpath.o: builtins/realpath.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_realpath 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/realpath.c -o $@; else rm -f $@; fi
obj/builtins/sleep.o: builtins/sleep.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_sleep 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/sleep.c -o $@; else rm -f $@; fi
obj/builtins/sort.o: builtins/sort.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_sort 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/sort.c -o $@; else rm -f $@; fi
obj/builtins/tail.o: builtins/tail.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_tail 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/tail.c -o $@; else rm -f $@; fi
obj/builtins/tee.o: builtins/tee.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_tee 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/tee.c -o $@; else rm -f $@; fi
obj/builtins/tr.o: builtins/tr.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_tr 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/tr.c -o $@; else rm -f $@; fi
obj/builtins/uniq.o: builtins/uniq.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_uniq 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/uniq.c -o $@; else rm -f $@; fi
obj/builtins/wc.o: builtins/wc.c $(HDR) obj/builtins
	@if grep -qi "^#define ENABLE_wc 1" config.h; then echo "$(CC)" "$(CFLAGS)"  -c "$<" "$@"; $(CC) $(CFLAGS) -c builtins/wc.c -o $@; else rm -f $@; fi

$(TARGET): $(OBJS)
	@echo "  $(CC) $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)"
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
