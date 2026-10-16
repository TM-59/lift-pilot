CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/flight.c src/svg.c src/render.c src/gif.c

.PHONY: all test plots clean
all: lift-pilot

lift-pilot: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_flight: $(SRC) tests/test_flight.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: test_flight test_gif
	./test_flight
	./test_gif

# Regenerates the CSV and the SVG charts shown in the README.
plots: lift-pilot
	mkdir -p docs
	./lift-pilot --headless docs
	./lift-pilot --gif docs/demo.gif
	head -c6 docs/demo.gif | grep -q GIF89a

clean:
	rm -f lift-pilot test_flight test_gif
