CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
# The terminal mode of src/main.c is POSIX; the feature macro is set here because a name starting
# with an underscore and a capital letter is reserved inside the program.
POSIX   := -D_POSIX_C_SOURCE=200809L
SRC     := src/flight.c src/svg.c src/render.c src/gif.c src/pitch.c
EMCC    ?= emcc
PYTHON  ?= python3
PORT    ?= 4215

.PHONY: all test bench story story-check web play smoke plots clean theory-figs theory-check
all: lift-pilot

lift-pilot: $(SRC) src/main.c
	$(CC) $(CFLAGS) $(POSIX) -o $@ $^ $(LDLIBS)

test_flight: $(SRC) tests/test_flight.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_pitch: src/flight.c src/pitch.c tests/test_pitch.c src/flight.h src/pitch.h tests/pitch_measure.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_observer: src/flight.c src/observer.c tests/test_observer.c src/flight.h src/observer.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

FERRY   := src/flight.c src/ferry.c
FERRY_H := src/flight.h src/ferry.h tests/ferry_policy.h tests/ferry_run.h

test_ferry: $(FERRY) tests/test_ferry.c $(FERRY_H)
	$(CC) $(CFLAGS) -o $@ $(FERRY) tests/test_ferry.c $(LDLIBS)

story_bench: $(FERRY) tests/story_bench.c $(FERRY_H)
	$(CC) $(CFLAGS) -o $@ $(FERRY) tests/story_bench.c $(LDLIBS)

# The story rows are compared byte for byte; generate and check them with the
# Debian 13 toolchain of the Dockerfile so libm rounds the same way everywhere.
test: test_flight test_pitch test_gif test_observer test_ferry story_bench
	./test_flight
	./test_pitch
	./test_gif
	./test_observer
	./test_ferry
	./story_bench --check docs/narrative

# Rewrites docs/narrative/{play-order,ablation,tension}.json.
story: story_bench
	mkdir -p docs/narrative
	./story_bench docs/narrative

# Regenerates every row into a scratch directory and compares the three files byte for byte.
story-check: story_bench
	rm -rf .story-check && mkdir .story-check
	./story_bench .story-check
	cmp .story-check/play-order.json docs/narrative/play-order.json
	cmp .story-check/ablation.json docs/narrative/ablation.json
	cmp .story-check/tension.json docs/narrative/tension.json
	rm -rf .story-check

bench: test_observer
	./test_observer
	@mkdir -p build
	./test_observer --trace > build/native-trace.csv

web: dist/physics.mjs
	cp web/index.html web/style.css web/app.mjs web/pitch-lab.mjs web/bgm.mjs dist/
	mkdir -p dist/bgm
	cp web/bgm/* dist/bgm/
	mkdir -p dist/art
	cp assets/art/* dist/art/
	$(PYTHON) scripts/check_size.py

dist/physics.mjs: src/flight.c src/observer.c src/flight.h src/observer.h src/ferry.c src/ferry.h src/ferry_web.c src/pitch.c src/pitch.h src/pitch_lab.c src/pitch_lab.h
	mkdir -p dist
	$(EMCC) $(CFLAGS) src/flight.c src/observer.c src/ferry.c src/ferry_web.c src/pitch.c src/pitch_lab.c -lm --no-entry -o $@ \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node -sFILESYSTEM=0 \
	  -sEXPORTED_FUNCTIONS=_lp_reset,_lp_controls,_lp_step,_lp_value,_fw_reset,_fw_tick,_fw_answer,_fw_value,_pl_reset,_pl_elevator,_pl_kick,_pl_step,_pl_value

play: web
	$(PYTHON) -m http.server $(PORT) --bind 127.0.0.1 --directory dist

smoke: web bench lift-pilot
	node scripts/smoke.cjs

# Regenerates the CSV and the SVG charts shown in the README.
plots: lift-pilot
	mkdir -p docs
	./lift-pilot --headless docs
	./lift-pilot --gif docs/demo.gif
	./lift-pilot --pitch docs/pitch.svg
	head -c6 docs/demo.gif | grep -q GIF89a

# Regenerates the plotted figures and the measured-values table of docs/theory (one command).
theory_figs: $(FERRY) src/pitch.c src/svg.c tests/theory_figs.c src/flight.h src/ferry.h src/svg.h src/pitch.h tests/pitch_measure.h
	$(CC) $(CFLAGS) -o $@ $(FERRY) src/pitch.c src/svg.c tests/theory_figs.c $(LDLIBS)

theory-figs: theory_figs
	mkdir -p docs/theory/figures
	./theory_figs docs/theory/figures

# Fails when a statement of docs/theory has no figure, a link is broken, or a referenced function or test is gone.
theory-check:
	$(PYTHON) scripts/theory_check.py

clean:
	rm -f lift-pilot test_flight test_pitch test_gif test_observer test_ferry story_bench
	rm -rf build dist

# ---- M61: local-only test standard (~/project-context/rules/test-standard.json) ----
# `make test` above stays the CI gate. `make check` runs every adopted kind through the
# common isolated runner (tm test) and writes git-ignored JUnit + test-results/summary.json.
TM_TEST_PYTHON ?= python3.14
TM_TEST_CMAKE ?= cmake
TM_TEST_CTEST ?= ctest
TM_TEST_NPM_CACHE ?= $(HOME)/.npm
TM_UNIT_BINS := test_flight test_pitch test_gif test_observer test_ferry story_bench

.PHONY: test-unit check test-trace test-perf test-e2e tm-ctest tm-deps test-tm-unit test-tm-trace test-tm-perf test-tm-e2e
test-unit:
	$(TM_TEST_PYTHON) scripts/local_test.py unit

check:
	$(TM_TEST_PYTHON) scripts/local_test.py all

test-trace test-perf test-e2e:
	$(TM_TEST_PYTHON) scripts/local_test.py $(@:test-%=%)

tm-ctest:
	mkdir -p test-results
	"$(TM_TEST_CMAKE)" -S tests/ctest -B .tm-ctest >/dev/null

test-tm-unit test-tm-trace: tm-ctest
	$(MAKE) -j1 $(TM_UNIT_BINS)
	"$(TM_TEST_CTEST)" --test-dir .tm-ctest -j 1 -L '^$(@:test-tm-%=%)$$' --output-on-failure --output-junit "$(CURDIR)/test-results/$(@:test-tm-%=%).xml"

test-tm-perf: tm-ctest
	"$(TM_TEST_CTEST)" --test-dir .tm-ctest -j 1 -L '^perf$$' --output-on-failure --output-junit "$(CURDIR)/test-results/perf.xml"

tm-deps:
	@test -n "$(TM_TEST_RUN_ID)" || { echo "Requires isolated tm test" >&2; exit 1; }
	mkdir -p .tm-home && touch .tm-home/user.npmrc .tm-home/global.npmrc
	@test -d node_modules/playwright || npm ci --offline --ignore-scripts --no-audit --no-fund --userconfig="$(CURDIR)/.tm-home/user.npmrc" --globalconfig="$(CURDIR)/.tm-home/global.npmrc" --cache="$(TM_TEST_NPM_CACHE)"

test-tm-e2e: tm-ctest tm-deps
	"$(TM_TEST_CTEST)" --test-dir .tm-ctest -j 1 -L '^e2e$$' --output-on-failure --output-junit "$(CURDIR)/test-results/e2e.xml"
# ---- Formatter, linter and type checker (no ignores) ----
# C: clang-format, clang-tidy (.clang-tidy) and the compiler with -Wall -Wextra -Wpedantic -Werror.
# Python (scripts/*.py): Ruff format, Ruff with every rule, mypy strict. These are pinned by uv.lock.
# JavaScript, CSS, HTML, JSON: Biome (recommended rules, warnings fail) and tsc --checkJs strict,
# pinned by package-lock.json. TIDY_EXTRA passes -isystem paths when the C library headers are elsewhere.
RUN        = uv run --locked
STYLE_C    = $(shell git ls-files '*.c' '*.h')
STYLE_TU   = $(shell git ls-files '*.c')
TIDY_EXTRA ?=
.PHONY: fmt lint typecheck check-style
fmt:
	$(RUN) clang-format -i $(STYLE_C)
	$(RUN) ruff format scripts
	$(RUN) ruff check --fix scripts
	npm run fmt
lint:
	$(RUN) clang-format --dry-run --Werror $(STYLE_C)
	$(RUN) clang-tidy $(STYLE_TU) -- -std=c11 -Isrc -Itests $(POSIX) $(TIDY_EXTRA)
	$(RUN) ruff format --check scripts
	$(RUN) ruff check scripts
	npm run lint
typecheck:
	for f in $(STYLE_TU); do $(CC) $(CFLAGS) $(POSIX) -Isrc -Itests -fsyntax-only $$f || exit 1; done
	$(RUN) mypy
	npm run typecheck
check-style: lint typecheck
