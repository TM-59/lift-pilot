# lift-pilot

Fly a motor glider in a terminal. A small C11 flight-dynamics simulator: point-mass longitudinal equations integrated with RK4, a parabolic drag polar with stall, and a side-view ASCII cockpit.
(C言語の飛行力学シミュレータ。端末上でグライダーを操縦して滑走路に軟着陸させます。)

## Run

```sh
make && ./lift-pilot            # interactive
./lift-pilot --snapshot         # one frame, no terminal needed
make test && make plots         # unit tests and docs/*.svg
docker build -t lift-pilot .    # same, reproducibly
```

Keys: `w`/`s` pitch (commanded angle of attack), `r`/`f` throttle, `p` pause, `q` quit. Land gently (sink < 2 m/s, path angle < 0.15 rad) on the `===` runway.

## Model

State `(x, h, V, γ, α)`; `α` follows the elevator command through a first-order lag.

```
V'  = (T − D)/m − g sin γ
γ'  = (L/m − g cos γ) / V
x'  = V cos γ
h'  = V sin γ
```

`CL = CL0 + CLα·α` up to the stall angle, then falls; `CD = CD0 + k·CL²` plus a stall term. Best glide ratio is `1/(2√(CD0·k))` ≈ 28.9.

## Results

Phugoid: a trimmed glide hit with +10 % airspeed trades speed for height in a slowly damped oscillation.

![speed](docs/speed.svg)
![track](docs/track.svg)
![polar](docs/polar.svg)
![ld](docs/ld.svg)

## Tests

`tests/test_flight.c` checks: numeric best L/D vs the analytic value (1 %), lift peak at stall, steady glide stays steady, energy conservation without drag, phugoid period vs Lanchester's `π√2·V/g` (10 %), touchdown rating, renderer bounds.

## Roadmap

See the Issues: short-period pitch dynamics, thermals, 3D view.

Part of a series of small C physics toys, tracked in [algo-vis-panel](https://github.com/TM-59/algo-vis-panel/issues/6). MIT licensed.
