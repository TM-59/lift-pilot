CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/flight.c src/svg.c src/render.c src/gif.c
EMCC    ?= emcc
PYTHON  ?= python3
PORT    ?= 4215

.PHONY: all test bench web play smoke plots clean
all: lift-pilot

lift-pilot: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_flight: $(SRC) tests/test_flight.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_observer: src/flight.c src/observer.c tests/test_observer.c src/flight.h src/observer.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

test: test_flight test_gif test_observer
	./test_flight
	./test_gif
	./test_observer

bench: test_observer
	./test_observer
	@mkdir -p build
	./test_observer --trace > build/native-trace.csv

web: dist/physics.mjs
	cp web/index.html web/style.css web/app.mjs dist/
	mkdir -p dist/art
	cp assets/art/* dist/art/
	$(PYTHON) scripts/check-size.py

dist/physics.mjs: src/flight.c src/observer.c src/flight.h src/observer.h
	mkdir -p dist
	$(EMCC) $(CFLAGS) src/flight.c src/observer.c -lm --no-entry -o $@ \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node -sFILESYSTEM=0 \
	  -sEXPORTED_FUNCTIONS=_lp_reset,_lp_controls,_lp_step,_lp_value

play: web
	$(PYTHON) -m http.server $(PORT) --bind 127.0.0.1 --directory dist

smoke: web bench lift-pilot
	node scripts/smoke.cjs

# Regenerates the CSV and the SVG charts shown in the README.
plots: lift-pilot
	mkdir -p docs
	./lift-pilot --headless docs
	./lift-pilot --gif docs/demo.gif
	head -c6 docs/demo.gif | grep -q GIF89a

clean:
	rm -f lift-pilot test_flight test_gif test_observer
	rm -rf build dist
