# Requirements

Written from the code, the tests and the bench output. Numbers come from `src/`, `tests/`,
`docs/narrative/` and the playability-gate measurement.

## Who, when, for what

- People curious about how a glider flies try, in a few minutes, how speed becomes height
  and why a steady glide lands softly. The flight laboratory (manual approach, trimmed landing,
  speed ↔ height) is for that, without a story.
- **The Last Ferry** (five hops, about 10–20 minutes) asks the same skill under pressure:
  reach each grass strip with a limited battery, in still air, sinking air or a headwind,
  while the club's last motor glider is flown down the valley to the man who bought her.
  Two choices change the flight that follows (the line past the quarry, and whether the
  motor is taken out at Mill Strip) and which of two endings you read.

## Devices and input

| Device | Input | What it does |
|---|---|---|
| PC | Keyboard | W/S angle of attack, R/F throttle, P pause (laboratory and ferry) |
| PC / phone | Pointer / touch | Sliders and the − / + buttons set the angle of attack and throttle; every scene answer, start, pause and reset is a button |
| Phone, upright | Touch only | The flight view comes first and fills about 62 % of the screen height; controls and instruments follow |
| Phone, sideways | Touch only | The flight view on the left at the screen height, controls and the ferry brief on the right |

Everything can be started and finished without a keyboard. There is no persistence, sound
or server API. The browser downloads only local files.

## Acceptance

### Physics and story (`make test`, `make bench`, `make story-check`, `make plots`)

- The flight model's own tests: best L/D within 1 % of 1/(2√(CD0·k)) ≈ 28.9, lift peak at the
  stall angle, steady glide stays steady, energy conservation without drag, phugoid period
  within 10 % of Lanchester's π√2·V/g, touchdown rating, renderer bounds.
- The browser adapter equals the flight model for 1,200 steps; native and WASM trajectories
  agree within 1e−8 (smoke); CSV/SVG/GIF outputs are unchanged by `make plots`.
- The ferry: in still air with no story numbers it equals `flight_step` frame for frame.
  Scene order hangar → weir → orchard → buyer → mill → doors → coast in every bench seed,
  both endings reached, every story number (`[mech]`), the quiet heavy scenes (`[quiet]`
  buyer, mill, doors) and both choices' trade (`[choice]`).
- `docs/narrative/*.json` must match a re-run in the Debian 13 toolchain byte for byte.

### Playability gate (iPhone-like, upright 375×812 and sideways 812×375, DPR 3, touch only)

| Item | Condition |
|---|---|
| ① Load | the canvas or text appears |
| ② Progress by tapping | the first goal (the ferry's first scene answered, the Kestrel airborne) within 25 s |
| ③ Console errors | 0, both orientations |
| ④ Words | every drawn word 14 CSS px or larger, canvas labels included |
| ⑤ Targets | every target 44×44 CSS px or larger (buttons, links, sliders, labels) |
| ⑥ Orientation | no sideways overflow; the flight view covers half the screen or more |
| ⑦ Direction | not applicable: the flight is longitudinal, there is no screen direction to steer |

## Not verified

A human first play, real phones (Safari), other browsers and a real GPU are not verified.
The emotion values in `docs/narrative/beats.json` are the author's; an independent review has
not been done. No public deployment has been made.
