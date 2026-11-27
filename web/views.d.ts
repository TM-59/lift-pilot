// Read-only observation objects the pages put on window for scripts/smoke.cjs and the playability
// gate. Used only by `tsc --checkJs`; nothing is emitted.

/** The C values of the running mode (laboratory or ferry; the other mode's names are absent). */
interface LiftState {
  x: number;
  h: number;
  v: number;
  gamma: number;
  alpha: number;
  time: number;
  command: number;
  throttle: number;
  lift: number;
  drag: number;
  ld: number;
  sink: number;
  energy: number;
  status: number;
  runway: number;
  onRunway: number;
  mode: number;
  stall: number;
  hop: number;
  scene: number;
  banner: number;
  strip: number;
  length: number;
  startH: number;
  airLift: number;
  wind: number;
  charge: number;
  motor: number;
  low: number;
  lowOn: number;
  need: number;
  outlandings: number;
  route: number;
  pulled: number;
  ending: number;
  loud: number;
  heavy: number;
  groundSpeed: number;
  making: number;
  hopTime: number;
  trailered: number;
  outHop: number;
}

/** One heavy scene as it opened: whether a banner or a loud readout was still showing. */
interface HeavyEntry {
  id: string;
  banner: number;
  loud: number;
  bannerShown: boolean;
}

/** window.liftView.snapshot. */
interface LiftSnapshot extends LiftState {
  paused: boolean;
  ferry: boolean;
  artReady: boolean;
  samples: number;
  sceneId: string | null;
  sceneText: string | null;
  bannerShown: boolean;
  bannerText: string | null;
  heavyLog: HeavyEntry[];
  welcome: boolean;
  inputCommand: number;
  inputThrottle: number;
  guide: { goal: string; next: string; tone: string; target: string | null };
}

/** The C values of the pitch laboratory (src/pitch_lab.c, by name). */
interface PitchLabState {
  time: number;
  alpha: number;
  q: number;
  theta: number;
  gamma: number;
  v: number;
  elevator: number;
  trimAlpha: number;
  margin: number;
  spWn: number;
  spZeta: number;
  approxWn: number;
  approxZeta: number;
  phPeriod: number;
  phTheory: number;
  oscillatory: number;
  departed: number;
  re0: number;
  re1: number;
  re2: number;
  re3: number;
  im0: number;
  im1: number;
  im2: number;
  im3: number;
}

/** window.pitchLab.snapshot. */
interface PitchLabSnapshot extends PitchLabState {
  running: boolean;
  experiment: string;
  samples: number;
  unstable: boolean;
  lambda: number;
  verdict: string | null;
  status: string | null;
}

/** Common test hook of the playability gate (read-only; canvas pixels). */
interface TmGate {
  version: number;
  readonly logical: { w: number; h: number };
  scene(): string;
  goal(): boolean;
  objective(): string | null;
  hint(): string | null;
  texts(): unknown[];
  targets(): unknown[];
  player(): { x: number; y: number } | null;
  nextTarget(): { x: number; y: number } | null;
}

interface Window {
  /** The music player (web/bgm.mjs) for scripts/bgm-smoke.cjs. */
  readonly musicView?: BgmView;
  liftView?: { snapshot: LiftSnapshot; engine: unknown };
  pitchLab?: { snapshot: PitchLabSnapshot };
  __tmGate?: TmGate;
}

/** What the music player asks for and what it plays (web/bgm.mjs). */
interface BgmView {
  cue: { track: string; gain: number };
  want: string | null;
  playing: string;
  gain: number;
  muted: boolean;
  unlocked: boolean;
  running: boolean;
  fetched: string[];
  loaded: string[];
  failed: string[];
  /** Output RMS in dBFS. */
  level: number;
}
