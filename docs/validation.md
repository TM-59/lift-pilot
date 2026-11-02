# M4 validation

Measured on 2026-10-26. Artwork changes are accompanied by actual C/WASM and UI checks.

| Check | Result |
|---|---|
| `make test` | Existing flight-model and GIF tests pass; observer adds 3,613 checks. |
| `make bench` | Direct flight model agrees with adapter for 1,200 steps in manual flight; trimmed landing at 93.42 s, x=2,676.437815 m, sink=1.070506 m/s. Hard landing, stall, clamping, invalid input and stopped simulation also checked. |
| Native / WASM trace | All 903 samples × 18 values from manual, trimmed and phugoid trajectories agree within 1e−8; maximum absolute difference 5.0023e−12. |
| `make plots` | All 6 original CSV/SVG/GIF files are byte-identical after regeneration. `src/flight.c` and the terminal/GIF implementation are unchanged. |
| `make web` | 7 files, 898,819 B including 2 original artworks; WASM 11,459 B. Below 10,000,000 B. |
| `make smoke` | 30 checks in 72.057 s; console/page errors 0, external requests 0. Real keyboard and touch input, pause, reset, landing, hard landing, stall, demo control locking, 120 s observation completion, field-note navigation, 390 px layout and 44 px buttons checked. |
| Failure states | Missing artwork leaves the real simulation usable with a labelled position marker; failed WASM loading produces an explicit error with controls disabled. Expected diagnostics in the separately injected WASM-failure context are excluded from the healthy-page error count. |

Native and browser execution use the same `src/flight.c` through `src/observer.c`.
JavaScript does not duplicate the flight equations. The regression trajectory uses
commands at known steps; the browser-control checks use actual UI input and a controlled
browser clock. The clock speeds up observation; it does not replace the physical state.
The original phugoid-period test still checks Lanchester's approximation within 10%.

## Captures

All pictures in `docs/images/` were inspected. They are screenshots of the running
browser/WASM build, except `before-terminal.jpg`, which captures the unmodified native
`--snapshot` output in a monospaced terminal-style viewer. No generated UI mockup was used.

- `observatory.jpg`: initial 100 m approach, paused.
- `manual-flight.jpg`: 5° command / 5% throttle, measured forces and path.
- `landing.jpg`: the actual 93.42 s trimmed touchdown, 0 m altitude and on-runway result.
- `phugoid.jpg`: 30 s of the real speed–height experiment, with measured airspeed trace.
- `stall.jpg`: 20° command, measured alpha 19.9°, actual stall state.
- `mobile.jpg`: 390 px touch viewport, after a real tap changed the command to 5°.
- `before-terminal.jpg`: original renderer, retained as before evidence.

The decorated scene uses a schematic ground line; its painted mountains are not collision
geometry. The plot below it carries the quantitative axes, units and runway marker.
Control labels distinguish commanded angle of attack, actual angle of attack and flight-path
angle. Current L/D is about 26.8 at 4°, while the model's best theoretical L/D remains 28.9.

## Scope and limits

The repository had no local CLAUDE.md; one was added to describe the actual model and
the original-art exception. CI's existing Docker build was replaced by PR-only C tests,
retaining its path filter and concurrency cancellation. No new workflow was added.

The art-material proxy score does not evaluate aesthetic quality or educational value.
Human first-play evaluation, actual iPhone/Safari/other-browser compatibility, real-GPU
performance, real-time long sessions, publication and external submission are unverified.
There is no audio, persistence or server API to migrate. No deployment was performed.

## M5: The Last Ferry and phone play (2026-11-03)

| Check | Result |
|---|---|
| `make test` (Debian 13, gcc 14.2) | existing tests unchanged; `test_ferry`: still air with no story numbers equals `flight_step` for 1,500 frames, scene order in 12 seeds, 7 `[mech]`, 3 `[quiet]`, 2 `[choice]`; story bench check 8 rows verbatim, all run keys distinct |
| `make story-check` / `make plots` | full regeneration byte-identical; CSV/SVG/GIF unchanged |
| `make smoke` | 63 checks, page errors 0, external requests 0; the ferry flown twice through the page with real clicks and sliders (quarry + motor out, ridge + whole), 7 scenes in order, heavy scenes with no banner, both endings; 375×812 and 812×375 touch: scene text 14 px, answers 46 px, view 58 % / 64 % |
| Playability gate | 2/7 → 7/7 |
| `make web` | 7 files, 925,968 B |

The smoke's own pilot is crude (two fixed pitch settings and the motor on LOW) and outlanded
9 times in each run; it checks the page, not the difficulty. Difficulty is measured by the story
bench's pilot in `docs/narrative/tension.json`. A human first play is not verified.
