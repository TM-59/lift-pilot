# Design

## Files and responsibilities

| File | Responsibility |
|---|---|
| `src/flight.c`, `flight.h` | The physics canon: point-mass longitudinal model, parabolic polar with stall, RK4, touchdown rating. Unchanged by the story. |
| `src/observer.c`, `observer.h` | The flight laboratory's state for the browser (manual approach, trimmed landing, speed ↔ height). |
| `src/ferry.c`, `ferry.h` | The Last Ferry: hops, scenes, story numbers, the glide gauges, outlandings. Calls `flight_step` unchanged. |
| `src/ferry_web.c` | Browser bridge: `fw_reset`, `fw_tick(frames, command_deg, throttle)`, `fw_answer(option)`, `fw_value(field)`. |
| `src/main.c`, `render.c`, `svg.c`, `gif.c` | Terminal view, CSV/SVG plots and the GIF demonstration (unchanged). |
| `tests/test_flight.c`, `test_gif.c`, `test_observer.c` | The model, the GIF codec, the laboratory adapter. |
| `tests/test_ferry.c` | Story order, every story number, quiet heavy scenes, both choices; the ferry against `flight_step`. |
| `tests/ferry_policy.h`, `ferry_run.h`, `story_bench.c` | A human-like pilot, one whole run, and the bench that writes `docs/narrative/{play-order,ablation,tension}.json`. |
| `web/app.mjs` | Words of the scenes, drawing, input, the `liftView` snapshot and the `__tmGate` hook. No physics. |
| `scripts/smoke.cjs` | Native/WASM comparison, real clicks and touch through both modes, both ferry endings, phone layout, screenshots. |

## The Last Ferry

Five hops, strip to strip. Each hop starts at the top of the climb-out (a trimmed 4° glide at the
hop's start height) and ends with a touchdown. A touchdown between the threshold and the end of
the strip with sink below 2 m/s and a path angle below 0.15 rad makes the strip; anything else
(short, long, hard, or still flying after 360 s) is an **outlanding**: the hop is flown again from
the same height with what is left in the battery. The third outlanding in one hop ends it on a
trailer (no "strip made" banner). Every second of the clock is flown.

Scenes open at fixed progress points: the start of the run and the end of each hop.

| Scene (`FS_*`) | Opens | Story numbers (one `FE_*` bit each) |
|---|---|---|
| `hangar` | at the start | `FE_CELLS`: battery 5 → 26 s of full throttle |
| `weir` (choice) | Weir Strip made | `FE_ROUTE`: quarry = strip at 2,500 m, air sinking 0.5 m/s; ridge = strip at 3,500 m, climb-out 15 m higher |
| `orchard` | Orchard Strip made | `FE_ORCHARD`: +15 s of charge |
| `buyer` (heavy) | after the orchard | `FE_WIND`: 3 m/s on the nose for hops 4 and 5 |
| `mill` (choice, heavy) | Mill Strip made | `FE_MOTOR`, pulled: no motor, CD0 0.015 → 0.014, the mill's winch to 145 m |
| `doors` (heavy) | Ferry Point made | `FE_DOORS`: the sea breeze (2 m/s more on the nose, air sinking 0.15 m/s), a 250 m yard |
| `coast` (ending) | the yard made | two endings by the mill choice |

Wind and vertical air motion move the whole air mass: after each unchanged RK4 step the
position is moved by `wind·dt` and the height by `lift·dt`. The glider's equations are not
touched. With no story numbers the strips are 600 m long, in still air, at 2,500 / 3,000 / 2,600 /
3,000 / 2,800 m from start heights of 82 / 120 / 98 / 80 / 88 m.

### The two needles

- **Glide needed** = (distance to 80 m past the threshold) / energy height, where energy height
  = h + (V² − 25²)/(2g), as a total-energy gauge reads it.
- **Glide made** = ground covered over energy height lost in each step, smoothed with a 5 s lag,
  capped at 60 (a climb reads 60).
- **LOW** is on while the glide needed is steeper than the glide being made. Seconds with LOW on
  are the `low` outcome in the bench; outlandings add to `seconds`.

### Heavy scenes

`buyer`, `mill` and `doors` clear any banner when they open, are shown on a dark, still panel and
darken the flight view; no "strip made" banner is raised for the hop that leads to them, nor in the
frame after they close (`[quiet]` in `tests/test_ferry.c`, `quiet` in every play-order seed).

## The goal and the next step

`guide()` in `web/app.mjs` turns the C state into two lines above the view: the goal (where to land,
how far, the strip length; in the laboratory the runway and the 2 m/s limit) and the next step. The
next step is the first that applies of: paused, stall (nose down), past the strip end, near the
strip and low with sink over 1.8 m/s (flare: nose up), on final with the motor running (motor off),
on final, LOW with battery (Motor 100%), climbing on the motor, LOW without a motor, the motor
running with height in hand (motor off), the nose above 9° or below 1° (back toward 4°), gliding
well. The control it names gets `.next-target`: the phone's quick row when it is on screen, else the
control panel. Only words come from JavaScript; every number is read from C.

## Screen states (`window.__tmGate.scene()`)

`loading` → `title` (welcome card) → `scene:<id>` ↔ `play` / `paused` → `scene:coast` → `done`.
The laboratory uses `play` / `paused` / `done` without scenes.

## Input

| Action | Keyboard | Pointer / touch |
|---|---|---|
| Angle of attack | W / S (±1°) | slider, − / + buttons, the quick row's − Nose / + Nose (upright phones) |
| Throttle | R / F (±5 %) | slider, the quick row's Motor 0% / Motor 100% (disabled with no motor or a flat battery) |
| Pause / resume | P | the main button |
| Scene answers | — | buttons in the scene panel |

## Test hook

`window.__tmGate`: `version`, `logical` (the scene canvas's CSS size), `scene()`, `goal()` (the
hangar answered and the Kestrel airborne), `objective()` and `hint()` (the words of `#goal-line` and
`#next-step`), `texts()` and `targets()` empty (all words and targets are DOM or canvas text the gate
measures directly), `player()` null, `nextTarget()` (the centre of the outlined control the next step
names: the welcome button, the open scene's first answer, Resume, nose up/down or the motor). `window.liftView.snapshot` exposes the C
state, the open scene, banner and heavy-scene log for the smoke test.

## Known limits

The pilot in the bench is a model of a human (it reads the same two needles, holds an attitude,
rounds out near the ground); it is not a player. The motor glider climbs at about 1.5 m/s at full
throttle; there are no airbrakes, so being high is fixed by diving early or flying a longer line.
A human first play, real phones and a real GPU are not verified.
