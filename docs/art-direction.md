# M4: a quiet flight laboratory

2026-10-26. Direction: original gouache aviation illustration, pale sky, sage and
cream aircraft, restrained ochre accents, with measured plots on a separate paper surface.
The calm setting suits a still-air point-mass glider; no fictional enemies or characters
were added to a physics simulator to increase an artwork score.

## Before and after

The original terminal renderer uses `>` / `/` / `\` for the aircraft and `===` for a runway.
`docs/images/before-terminal.jpg` captures the actual `--snapshot` output.
The original terminal renderer, GIF, CSV and SVG outputs remain available.

The browser now draws the original aircraft sprite at the C state position, with attitude
derived from flight-path angle plus angle of attack. The generated scenery is illustrative:
it does not determine terrain, contact, altitude or distance. A dashed schematic ground
line and the separate measured plot make this distinction visible.

## Generated assets: 2 generated, 2 accepted

Generator: Codex built-in image_gen, using the subscription tool. Original designs;
no third-party image, sound or font files. Both generated images were visually inspected.
The background was converted to JPEG for distribution; the sprite retains transparency.

| Asset | Inspection and actual use |
|---|---|
| `assets/art/airfield.jpg` | Accepted: pale blue sky, layered mountains, meadow and small hangar; no gibberish text or painted instruments. Drawn as the flight-view background. |
| `assets/art/glider-sprite.png` | Accepted: coherent white/sage fuselage, orange tail, canopy, wing pair and wheel; faces right, transparent background. Slightly elevated side perspective is illustration, not an aerodynamic mesh. Drawn as the moving aircraft. |

### Background prompt

Use case: stylized-concept. Asset type: wide background painting for a small flight-dynamics
simulator called Lift Pilot. Make a refined editorial gouache painting, wide landscape
1536x1024. Alpine foothills and a quiet grass airfield at early morning, distant layered
blue mountains, sage-green valley, golden sun on ridgelines, thin airy clouds in a large
pale cream-blue sky. The bottom quarter is a low, mostly flat meadow with a tiny hangar
far right. Strong sense of space and calm silent flight. Paint texture, elegant limited
palette of cream, desaturated teal, slate blue and muted ochre. The central sky is completely
empty so a separately rendered moving aircraft and instrumentation can be placed over it.
No aircraft, no people, no lettering, no logos, no HUD, no graphs, no text, no border.
Original independent art, no recognizable branded designs. This is scenery only; do not
illustrate gauges or numerical physical quantities.

### Aircraft prompt

Use case: stylized-concept. Asset type: one clean transparent sprite for a side-view
scientific gliding simulator. One original unbranded white-and-sage motor glider, entire
aircraft visible with generous margin, nose pointing RIGHT. Exact lateral side elevation
seen from slightly above the wing plane so slender long wings remain recognizable but
its flight direction is clearly horizontal. Slim aerodynamic white fuselage, small
translucent blue canopy, tiny single landing wheel just under cockpit, single vertical
tail at LEFT with a muted burnt-orange tip, very long slender white wings with sage accents.
Calm refined hand-painted gouache illustration, crisp silhouette at small screen size,
soft natural shadows within the body. The craft is level, not tilted or banking. No
spinning propeller, no terrain, no shadow cast onto background, no people outside the
canopy, no labels, no lettering, no logo, no border, no gauges, no text, no watermark.
Fully transparent background, not white. Only a single aircraft. This original design
is not any named commercial aircraft.

## Descriptions and scientific reference

A local gemma4:e4b draft produced two short descriptions after an empty first response.
Both descriptions were checked against the actual images; neither supplies physical facts:
“Gouache alpine valley with pale blue sky and green grass.” and
“White and sage motor glider with orange tail, facing right.”

The explanatory copy was checked against NASA Glenn's
[Lift to Drag (L/D) Ratio](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/lift-to-drag-l-d-ratio/)
(accessed 2026-10-26): for steady unpowered glide, larger L/D gives a shallower glide angle.
The live L/D display uses the actual coefficient ratio, not a constant best-glide value.
The picture is not experimental evidence or a real airfield photograph.

## Limits

No human first-play evaluation, real mobile-device test or real-GPU performance measurement.
Aircraft damage and separate landing illustrations are not implemented; landing and stall
are shown through actual model state, text and instrumentation. No publication or submission.
