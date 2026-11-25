# Agent rules for open-legio

## Commands

- `npm run setup` once; `npm run build` after every change. The build uses `-Wall -Wextra -Werror`, so a
  warning is a failure.
- `npm run flash [-- <name>]` writes `build/ship/<name>/<name>.bin` over USB DFU. The name may be omitted
  while `build.json` lists one firmware.
- `npm run debug [-- <name>] [test|debug|ship] [--probe usb|jlink|stlink]` builds (default `test`),
  loads it and prints its RTT log; `bug_print` output appears there. The default `usb` needs no
  probe: it flashes over USB DFU and reads the log over USB.
- `.claude/skills/legio-flash` and `.claude/skills/legio-debug` cover these commands, their output
  and their failure modes; read the matching one before building, flashing or debugging.
- `.claude/skills/legio-crash` covers a crash, lockup or reboot loop: read the `hardfault`/`bt` report
  in the log, or attach with a probe without resetting when the core locked up. Read it before touching
  a module that has stopped.
- `.claude/skills/legio-new-firmware` covers starting a firmware from the example and what the board
  calls on the DSP. Read it before writing a new firmware.

## Writing a firmware

- A firmware is `src/vi_<name>.cpp`, whose `main` returns `lac::legio_main<dsp_t, 96000>()`
  (`96000` is the codec rate, 48000 or 96000), and the DSP header
  `src/vi_<name>.h`. Add `{ "name": "<name>", "sources": ["src/vi_<name>.cpp"] }` to
  `build.json`'s `firmwares`.
- Copy `src/open_legio_main.cpp` and `src/vi_example.h` as the starting point. Change only the firmware files; everything under
  `src/lacerti/` and the other `src/` directories is board support.
- The target is a Cortex-M7 with an FPv5 FPU, built with `-ffast-math`, `-Wdouble-promotion`,
  `-fno-exceptions` and `-fno-rtti`: use `float`, no `throw`, no heap allocation, no NaN or infinity.

## The `dsp_t` contract

`legio_brd<dsp_t, HARDWARE_FRAME_RATE>` runs the DSP once per block of `FRAME_COUNT` stereo frames:
`control(ctl)`, then `operator()` per frame.

- Constants: `FRAME_COUNT`, `COLOR_LEFT`, `COLOR_RIGHT` (`numerus::color`), `ENC_ACCEL`.
- `ctl_t`: constructible from `legio_inp` (`src/lacerti/prd/legio/legio_inp.h`): knobs `k1` `k2`,
  pitch `p0`, switches `swl` `swr` (0–2), `sync_trig`, encoder `enc` (tuning in whole octaves), `enc_moved` (encoder turned or pressed this block), `enc_norm` (0–1, press recentres), `enc_hold` (0–1, saved, never reset),
  `tap_toggle` (saved press latch).
- `void set_frame_rate(float fps, alloc_t*)`: called before audio starts; the only place to allocate.
- `legio_out control(const ctl_t&)`: per block; returns the two LED colors. The firmware owns its LEDs, so a muting firmware returns red.
- `std::array<float, 2> operator()(std::array<float, 2> in)`: per frame.
