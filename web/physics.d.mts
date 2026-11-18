// Types of the WebAssembly module that `make web` builds as dist/physics.mjs from the C sources
// (EXPORTED_FUNCTIONS in the Makefile). Used only by `tsc --checkJs`; nothing is emitted.

export interface FlightModule {
  _lp_reset(mode: number): void;
  _lp_controls(commandDegrees: number, throttle: number): void;
  _lp_step(frames: number): void;
  _lp_value(field: number): number;
  _fw_reset(): void;
  _fw_tick(frames: number, commandDegrees: number, throttle: number): void;
  _fw_answer(option: number): number;
  _fw_value(field: number): number;
  _pl_reset(staticMargin: number): void;
  _pl_elevator(offsetDegrees: number): void;
  _pl_kick(qDegrees: number): void;
  _pl_step(frames: number): void;
  _pl_value(field: number): number;
}

export default function createFlight(): Promise<FlightModule>;
