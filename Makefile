CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/flight.c src/svg.c src/render.c src/gif.c src/pitch.c
EMCC    ?= emcc
PYTHON  ?= python3
PORT    ?= 4215

.PHONY: all test bench story story-check web play smoke plots clean theory-figs theory-check
all: lift-pilot

lift-pilot: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

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
	cp web/index.html web/style.css web/app.mjs web/pitch-lab.mjs dist/
	mkdir -p dist/art
	cp assets/art/* dist/art/
	$(PYTHON) scripts/check-size.py

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
	$(PYTHON) scripts/theory-check.py

clean:
	rm -f lift-pilot test_flight test_pitch test_gif test_observer test_ferry story_bench
	rm -rf build dist
