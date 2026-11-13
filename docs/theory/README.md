# Theory handbook

Why the program is correct, from the repository alone: every law, numerical rule and game rule the code relies on, each stated in Markdown **with a figure**, the place in the code that implements it, and the test or measurement that checks it.

- SI units: metres, seconds, kilograms, newtons, radians (degrees where a control or an axis says so).
- Plotted figures and the numbers quoted in the text are produced by the flight model itself: `make theory-figs` rewrites [figures/](figures/) and [figures/values.md](figures/values.md). Diagrams are Mermaid, written in the pages.
- `make theory-check` fails when a statement has no figure, a link is broken, a referenced function or test no longer exists, or a public function is missing from the tables below.

## Pages

1. [Aerodynamic forces](01-aerodynamics.md) — lift and drag, the lift curve and the stall, the drag polar, best glide
2. [Equations of motion](02-motion.md) — point-mass motion, the steady glide and the speed polar, energy, the phugoid, the angle-of-attack lag
3. [The numerical scheme, the ground and the screen](03-numerics-and-ground.md) — the Runge–Kutta step, its accuracy, the touchdown rule, the path to the cockpit
4. [The Last Ferry: the rules of the game layer](04-last-ferry.md) — moving air, the battery, the two glide gauges, outlandings, scenes
5. [Pitch dynamics: the rigid body](05-pitch.md) — pitching moment and static margin, the rigid-body equations, the short period and the phugoid from the eigenvalues, the elevator step, divergence behind the neutral point, the RK4 step limit, the pitch laboratory

## How the pieces depend on each other

```mermaid
flowchart TD
    S02["S02 lift curve and stall"] --> S01["S01 lift and drag"]
    S03["S03 drag polar"] --> S01
    S02 --> S03
    S03 --> S04["S04 best glide ratio"]
    S01 --> S05["S05 point-mass motion"]
    S05 --> S06["S06 steady glide, speed polar"]
    S04 --> S06
    S05 --> S07["S07 energy"]
    S07 --> S08["S08 phugoid"]
    S06 --> S08
    S05 --> S09["S09 angle-of-attack lag"]
    S05 --> S10["S10 Runge–Kutta step"]
    S09 --> S10
    S10 --> S11["S11 fourth-order accuracy"]
    S10 --> S12["S12 touchdown rule"]
    S12 --> S13["S13 model to screen"]
    S10 --> S14["S14 moving air"]
    S06 --> S14
    S05 --> S15["S15 battery"]
    S07 --> S16["S16 energy height, glide needed"]
    S16 --> S17["S17 glide made, LOW"]
    S14 --> S17
    S12 --> S18["S18 strip, outlanding, trailer"]
    S18 --> S19["S19 scenes"]
    S15 --> S19
    S01 --> S20["S20 pitching moment, static margin"]
    S05 --> S21["S21 rigid-body equations"]
    S20 --> S21
    S21 --> S22["S22 linearisation, two modes"]
    S22 --> S23["S23 short-period approximation"]
    S22 --> S24["S24 rigid-body phugoid"]
    S08 --> S24
    S22 --> S25["S25 response follows the eigenvalue"]
    S20 --> S26["S26 elevator step, new trim"]
    S22 --> S27["S27 divergence behind the neutral point"]
    S11 --> S28["S28 RK4 step limit"]
    S22 --> S28
    S21 --> S29["S29 pitch laboratory"]
    S13 --> S29
```

## Coverage: statement → page → figure → code → test

| ID | Statement | Page | Figure | Code | Test |
|---|---|---|---|---|---|
| S01 | Lift and drag $qSC_L$, $qSC_D$ | [1](01-aerodynamics.md) | `figures/forces.svg` | `src/flight.c#lift_force`, `src/flight.c#drag_force`, `src/flight.c#glider_default` | `tests/test_flight.c#test_steady_glide_stays_steady` |
| S02 | Lift curve and stall | [1](01-aerodynamics.md) | `figures/coefficients.svg` | `src/flight.c#lift_coeff` | `tests/test_flight.c#test_lift_peaks_at_the_stall_angle` |
| S03 | Drag polar $C_{D0} + kC_L^2$ | [1](01-aerodynamics.md) | `figures/polar.svg` | `src/flight.c#drag_coeff` | `tests/test_flight.c#test_best_glide_matches_the_polar` |
| S04 | Best glide ratio $1/(2\sqrt{C_{D0}k})$ | [1](01-aerodynamics.md) | `figures/glide-ratio.svg` | `src/flight.c#best_glide_ratio` | `tests/test_flight.c#test_best_glide_matches_the_polar` |
| S05 | Point-mass equations of motion | [2](02-motion.md) | mermaid | `src/flight.c#derivative` | `tests/test_flight.c#test_energy_is_conserved_without_drag` |
| S06 | Steady glide and the speed polar | [2](02-motion.md) | `figures/speed-polar.svg` | `src/flight.c#trim_glide` | `tests/test_flight.c#test_steady_glide_stays_steady` |
| S07 | Energy: $\dot e = (T - D)V/m$ | [2](02-motion.md) | `figures/energy.svg` | `src/flight.c#specific_energy` | `tests/test_flight.c#test_energy_is_conserved_without_drag` |
| S08 | Phugoid period $\pi\sqrt2\,V/g$ | [2](02-motion.md) | `figures/phugoid.svg` | `src/flight.c#flight_step` | `tests/test_flight.c#test_phugoid_period_follows_lanchester` |
| S09 | Angle-of-attack lag | [2](02-motion.md) | `figures/lag.svg` | `src/flight.c#derivative` | `tests/theory_figs.c#lag-at-0.24s` |
| S10 | Runge–Kutta step | [3](03-numerics-and-ground.md) | mermaid | `src/flight.c#flight_step` | `tests/test_observer.c#lp_value(LP_X)` |
| S11 | Fourth-order accuracy | [3](03-numerics-and-ground.md) | `figures/rk4.svg` | `src/flight.c#flight_step` | `tests/theory_figs.c#rk4-order` |
| S12 | Touchdown: landing or crash | [3](03-numerics-and-ground.md) | `figures/touchdown.svg` | `src/flight.c#check_touchdown` | `tests/test_flight.c#test_touchdown_rating` |
| S13 | From the model to the screen | [3](03-numerics-and-ground.md) | mermaid | `src/observer.c#lp_reset`, `src/observer.c#lp_controls`, `src/observer.c#lp_step`, `src/observer.c#lp_value`, `src/render.c#render_frame` | `tests/test_observer.c#lp_reset(1)` |
| S14 | Moving air | [4](04-last-ferry.md) | `figures/air.svg` | `src/ferry.c#ferry_tick` | `tests/test_ferry.c#[flight] still air` |
| S15 | The battery | [4](04-last-ferry.md) | `figures/battery.svg` | `src/ferry.c#ferry_tick` | `tests/test_ferry.c#[mech] hangar` |
| S16 | Energy height and the glide needed | [4](04-last-ferry.md) | `figures/gauges.svg` | `src/ferry.c#ferry_need` | `tests/theory_figs.c#need-start` |
| S17 | Glide made and the LOW gauge | [4](04-last-ferry.md) | mermaid | `src/ferry.c#ferry_low_on`, `src/ferry.c#glide_step` | `tests/test_ferry.c#[mech] all story numbers off` |
| S18 | Strip, outlanding, trailer | [4](04-last-ferry.md) | mermaid | `src/ferry.c#outland`, `src/ferry.c#clear_hop` | `tests/test_ferry.c#[mech] outlanding` |
| S19 | Scenes and the numbers they change | [4](04-last-ferry.md) | mermaid | `src/ferry.c#ferry_init`, `src/ferry.c#ferry_answer`, `src/ferry.c#ferry_options`, `src/ferry.c#ferry_heavy`, `src/ferry.c#ferry_ending`, `src/ferry.c#ferry_value` | `tests/test_ferry.c#[order]` |
| S20 | Pitching moment, $C_{m\alpha} = -C_{L\alpha}\,\text{SM}$, trim | [5](05-pitch.md) | `figures/pitch-moment.svg` | `src/pitch.c#pitch_default`, `src/pitch.c#pitch_cm_alpha`, `src/pitch.c#pitch_moment_coeff`, `src/pitch.c#pitch_trim_elevator`, `src/pitch.c#pitch_trim_alpha` | `tests/test_pitch.c#test_trim_is_an_equilibrium` |
| S21 | Rigid-body equations $\dot\theta = q$, $I\dot q = M$ | [5](05-pitch.md) | mermaid, `figures/pitch-free.svg` | `src/pitch.c#pitch_step`, `src/pitch.c#pitch_alpha`, `src/pitch.c#pitch_trim` | `tests/test_pitch.c#test_energy_is_conserved_without_drag` |
| S22 | Linearisation: short period and phugoid | [5](05-pitch.md) | `figures/root-locus.svg` | `src/pitch.c#pitch_jacobian`, `src/pitch.c#pitch_eigen`, `src/pitch.c#pitch_modes` | `tests/test_pitch.c#test_eigen_solver_on_a_known_matrix` |
| S23 | Short-period approximation $\omega^2 = -M_\alpha - ZM_q$ | [5](05-pitch.md) | `figures/short-period-frequency.svg`, `figures/short-period-damping.svg` | `src/pitch.c#pitch_short_period_approx` | `tests/test_pitch.c#test_two_modes_and_the_short_period_approximation` |
| S24 | Rigid-body phugoid, Lanchester $\times\sqrt{1 + Z\kappa}$ | [5](05-pitch.md) | `figures/pitch-phugoid.svg` | `src/pitch.c#pitch_phugoid_theory` | `tests/test_pitch.c#test_phugoid_without_pitch_damping_is_lanchester` |
| S25 | Simulated response follows the eigenvalue | [5](05-pitch.md) | `figures/sp-response.svg` | `src/pitch.c#pitch_step` | `tests/test_pitch.c#test_response_matches_the_eigenvalue` |
| S26 | Elevator step and the new trim | [5](05-pitch.md) | `figures/elevator-step.svg` | `src/pitch.c#pitch_trim_alpha`, `src/main.c#pitch_chart` | `tests/test_pitch.c#test_elevator_step_settles_at_the_moment_balance` |
| S27 | Divergence behind the neutral point | [5](05-pitch.md) | `figures/divergence.svg` | `src/pitch.c#pitch_eigen` | `tests/test_pitch.c#test_negative_static_margin_diverges` |
| S28 | RK4 stability limit on the short period | [5](05-pitch.md) | `figures/pitch-rk4.svg` | `src/pitch.c#pitch_step` | `tests/test_pitch.c#test_rk4_stability_limit` |
| S29 | From the pitch model to the pitch laboratory | [5](05-pitch.md) | mermaid | `src/pitch_lab.c#pl_reset`, `src/pitch_lab.c#pl_elevator`, `src/pitch_lab.c#pl_kick`, `src/pitch_lab.c#pl_step`, `src/pitch_lab.c#pl_value` | `scripts/smoke.cjs#_pl_value` |

Tests named `tests/theory_figs.c#…` are measurements recorded in [figures/values.md](figures/values.md) by `make theory-figs`, not assertions of `make test`. S09, S11 and S16 are covered only that way; turning them into assertions is a follow-up. Writing S17 down exposed a defect in the start value of the glide-made readout (issue #15), since fixed; the story numbers were retuned with it (see `docs/validation.md`).

## Public functions that state no law

| Function | What it is |
|---|---|
| `src/svg.c#svg_plot` | writes the line charts of this handbook and of `make plots` |
| `src/gif.c#gif_open`, `src/gif.c#gif_frame`, `src/gif.c#gif_close`, `src/gif.c#gif_text_size`, `src/gif.c#gif_draw_text` | GIF89a/LZW encoder for the demo animation |

## Reading the source against this table

Functions reviewed for this handbook: every declaration in `src/flight.h`, `src/observer.h`, `src/ferry.h`, `src/pitch.h`, `src/pitch_lab.h`, `src/render.h`, `src/svg.h` and `src/gif.h`, plus the static helpers `derivative`, `advance` and `dynamic_pressure` in `src/flight.c` and `energy_height`, `glide_step`, `launch`, `start_hop`, `outland`, `clear_hop` in `src/ferry.c`, and `pitch_derivative`, `pitch_advance` in `src/pitch.c`. `src/main.c` (terminal program and plots) and `src/ferry_web.c` (WebAssembly wrapper) call these functions and state no law of their own.
