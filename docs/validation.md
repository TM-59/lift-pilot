# M2: rigid-body pitch model (2026-11-14, issue #2)

A separate model, `src/pitch.c`, adds pitch attitude, pitch rate, a pitching moment (static margin, pitch damping, elevator) to the point-mass equations. `src/flight.c`, the laboratory and The Last Ferry are not changed: `docs/run.csv`, the four SVG charts, `docs/demo.gif`, the three files of `docs/narrative/` and the 13 earlier theory figures are byte for byte the same; the issue's sentence "replace the lag" was carried out as "add the model beside the lag" for that reason.

| Check (default glider, 4 deg glide, static margin 0.15 unless noted) | Measured | Theory | Relative difference |
|---|---|---|---|
| short-period natural frequency, eigenvalue vs two-state approximation | 4.19408 rad/s | 4.17731 rad/s | 4.0e-3 |
| short-period damping ratio | 0.648501 | 0.648067 | 6.7e-4 |
| the same frequency at static margin 0.05 | 3.26297 rad/s | 3.24463 rad/s | 5.7e-3 |
| damped frequency from the simulated pitch rate vs the eigenvalue | 3.19196 rad/s | 3.19259 rad/s | 2.0e-4 |
| damping ratio from successive peaks vs the eigenvalue | 0.64876 | 0.648501 | 4.0e-4 |
| phugoid period vs Lanchester with pitch damping | 16.9082 s | 16.838 s | 4.2e-3 |
| phugoid period with Cmq = 0 vs Lanchester | 13.0888 s | 12.9887 s | 7.7e-3 |
| steady alpha 3000 s after a 1 deg elevator step vs the moment balance | 5.4546 deg | 5.45455 deg | 9.7e-6 |
| growth rate at static margin -0.05 vs the positive real eigenvalue | 0.334745 /s | 0.334658 /s | 2.6e-4 |
| largest stable RK4 step, simulation vs amplification factor | 0.63339 s | 0.63339 s | 5.4e-8 |

The rows are written by `make theory-figs` into `docs/theory/figures/values.md` and asserted with tolerances in `tests/test_pitch.c` (9 tests). `make test`, `make story-check`, `make plots`, `make theory-check` (29 statements, 23 generated figures) pass. `make web` 978,612 B (8 files). `make smoke`: 160 checks in 1,876 s, page errors 0, external requests 0; the WebAssembly eigenvalues equal the native ones within 1e-5; the pitch laboratory was driven with real clicks and taps at 1440x1000, 1024x768, 390x844 and 844x390: smallest text 14 px, smallest target 44 px, no horizontal overflow, the elevator step ends within 0.3 deg of the moment balance after 12 s (the rest is the phugoid), and at static margin -0.05 the glider departs at 9.28 s.

Not verified: real phones and Safari, human first use of the pitch laboratory, and the model against a real aircraft (the pitch coefficients are plausible glider values, not measured ones).

# Glide-made readout fix and retune (2026-11-11, issue #15)

`launch()` passed the sink rate to `glide_step` with the wrong sign, so the glide-made needle started every hop at its cap (60) instead of the steady launch glide (26.8), and the LOW light could not come on for the first seconds. The bench pilot reads this needle; with the cap it dived off height at the start of each hop. With the needle right the pilot flies better, the two late punishments stopped costing time (buyer −0.1 %, doors −1.9 % against the 5 % the story test requires), and the Mill choice lost its trade-off. Six story numbers were therefore retuned by a search over 45 settings, each scored by the same conditions as `tests/test_ferry.c` (five passed; the one closest to the old numbers was taken).

| Number | Before | After |
|---|---|---|
| valley wind, hops 4 and 5 (`VALLEY_WIND`) | 3 m/s on the nose | 4 m/s |
| sea breeze on the last hop (`SEA_WIND`, `SEA_LIFT`) | 2 m/s more, air sinking 0.15 m/s | 3 m/s more, air sinking 0.25 m/s |
| quarry air (`QUARRY_LIFT`) | sinking 0.5 m/s | sinking 0.4 m/s |
| motor pulled (`CLEAN_CD0`, `WINCH_H`) | CD0 0.014, winch to 145 m | CD0 0.013, winch to 160 m |

| Check | main | this change |
|---|---|---|
| readout at launch (`making-start`) | 60 (the cap) | 26.76 = ground speed / sink; new `[gauge]` assertion within 1 % |
| `[mech]` buyer / doors, seconds, seeds one way (relative) | 32/40 (−8.4 %) / 34/40 (−8.7 %) | 34/40 (−12.4 %) / 33/40 (−8.1 %) |
| all story numbers off, seconds / low | 28/40 / 36/40 | 30/40 / 31/40 |
| `[choice]` weir seconds, low / mill seconds, low | 32, 35 / 28, 40 of 40 | 32, 39 / 28, 40 of 40 |
| tension, 192 runs: no failure / not completed | 34 (18 %) / 0 | 17 (9 %) / 0 |
| outlanding in a hop, share of runs, hops 1–5 | 0.21 0.11 0.14 0.46 0.47 | 0.26 0.14 0.14 0.54 0.48 |
| ablation, all effects on: mean seconds / time LOW / outlandings | 864 / 215 / 2.88 | 928 / 297 / 3.80 |
| narrative score (worst seed, median) | 93, 93 | 93, 93 |

`make test` (with the regenerated `docs/narrative`), `make story-check` (all rows byte for byte), `make theory-figs` + `make theory-check` (19 statements) and `make plots` pass; `src/flight.c` and the CSV/SVG/GIF outputs are unchanged. `make web` 935,727 B (7 files); `make smoke`: 127 checks in 981 s, page errors 0, external requests 0. The ferry was flown twice through the page with real clicks (quarry + motor out, ridge + whole): 7 scenes in order and both endings. The smoke's own crude pilot (two fixed pitch settings, motor on LOW) outlanded 9 times and rode the trailer on 3 hops on each path (on main: 9 outlandings / 3 trailers, 9 outlandings / 3 trailers).

The run is a little harder than before for the bench pilot (fewer flawless runs, about one more outlanding per run). Human first-play feel and real phones are not verified.

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
