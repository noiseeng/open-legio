---
name: legio-new-firmware
description: Start a new Legio firmware from the example by copying vi_example to vi_<name>, registering it in build.json and filling in the DSP the board drives (controls, per-frame audio, LEDs, buffers). Use when asked to make, start, add or write a new firmware, effect, oscillator or algorithm for Legio (e.g. "make a delay", "new firmware called fold", "add a second firmware", "what does the board call on my DSP"). To build and flash it, use `legio-flash`; to run it with a probe, `legio-debug`.
---

# legio-new-firmware

## Create it

1. Copy `src/open_legio_main.cpp` to `src/vi_<name>.cpp` and `src/vi_example.h` to `src/vi_<name>.h`.
2. In both, rename `vi_example` to `vi_<name>` (the struct, the include, and `NAME`).
3. Add `{ "name": "<name>", "sources": ["src/vi_<name>.cpp"] }` to `firmwares` in `build.json`.
4. `npm run build -- <name>`. With no name `build` builds every firmware; once `build.json` lists more
   than one, `flash` and `debug` need the name.

`src/vi_<name>.cpp` only runs the board with the DSP:

```cpp
int main() { return lac::legio_main<legio::vi_<name><8>, 96000>(); }
```

The DSP's template argument is `FRAME_COUNT`, the stereo frames per audio block. `96000` is the codec
rate the board runs at; keep it unless the DSP is too slow (`npm run debug` prints `cpu <n>%`), then use
`48000`. Keep all of the example's includes. Change only the two
firmware files; everything else under `src/` is board support.

## What the board uses

`lac::legio_brd<dsp_t, HARDWARE_FRAME_RATE>` (`src/lacerti/prd/legio/hardware/legio_brd.h`) reads these from the DSP and
nothing else:

- Constants:
  - `FRAME_COUNT`: frames per block.
  - `COLOR_LEFT`, `COLOR_RIGHT`: `numerus::color`; the boot colours of the two LEDs.
  - `ENC_ACCEL`: `bool`; encoder acceleration on the saved pitch.
- `ctl_t`, constructible from `const legio_inp&`.
- `template <class alloc_t> void set_frame_rate(float fps, alloc_t* alloc)`: called once before audio
  starts.
- `legio_out control(const ctl_t&)`: once per block, before the frames. It returns the two LED colours.
- `std::array<float, 2> operator()(std::array<float, 2> in)`: once per frame, stereo in to stereo out.

The example's `NAME`, `SHIPPED` and `IS_OSCILLATOR` are not read by anything; keep or drop them.

## Controls

`legio_inp` (`src/lacerti/prd/legio/legio_inp.h`) is what `ctl_t` is built from:

- Knobs `k1` (left), `k2` (right): 0–1.
- Pitch `p0`: octaves above C0 (`16.35f * exp2(p0)`), the pitch CV plus the encoder's saved tuning. Also
  `p0_norm`, `sp`, `fp`.
- Switches `swl`, `swr`: 0 bottom, 1 middle, 2 top.
- Sync input: `sync_gate` (held) and `sync_trig` (the debounced press, `hui.tac.pressed()`).
- Encoder: `enc` (tuning in whole octaves), `enc_moved` (the encoder turned or was pressed this block), `enc_norm` (0–1, press recentres), `enc_hold` (0–1, saved, never reset), push as `encs_gate` and
  `encs_trig` (`hui.enc.tac.pressed()`), and `tap_toggle` (saved press latch).
- The meters `meter_inpl`, `meter_inpr`, `meter_outl`, `meter_outr`.

The header's comments give every unit and range; don't guess one.

## Audio and LEDs

- Samples are `float`, nominally −1 to 1. The example clamps passthrough to that range.
- `control` returns `legio_out{led1, led2}`, where each is a `numerus::color`. The example builds them
  from 0–255 integer RGB (`to_color`).
- The firmware owns its LEDs, so a muting firmware returns red.
- `control` and `operator()` run inside the SAI DMA interrupt (half and complete transfer):
  keep them bounded and never wait in them.

## Memory

- There is no heap: no `new`, `malloc` or `std::vector`. Fixed sizes are `std::array` members.
- Buffers whose size depends on the frame rate (delay lines, loopers) come from the `alloc` passed to
  `set_frame_rate`. `alloc->alloc_array_init(count, &ptr, "name")` fills `ptr` and returns it, with every
  element zeroed or default-constructed.
  - It takes memory from the fastest zone with room, in this order: DTCM (128 KB less the stack), ITCM
    (64 KB), SRAM1 (512 KB), SRAM2 (288 KB), SRAM4 (64 KB), then the 64 MB external SDRAM. A buffer
    too big for internal RAM (a multi-second delay or looper) lands in SDRAM. SDRAM is cached but
    slower than internal RAM.
  - It can't be freed: `free` asserts. Allocate once, in `set_frame_rate`.

## Target rules

Cortex-M7 with an FPv5 FPU (`-mfpu=fpv5-d16`), `-ffast-math`, no exceptions, no RTTI, `-Wall -Wextra
-Werror`:

- Use `float`. `-Wdouble-promotion` with `-Werror` makes an implicit promotion to `double` a build
  error, and `-fsingle-precision-constant` makes unsuffixed literals `float`.
- `<cmath>` is available (newlib's libm): `std::exp2`, `std::sin`, `std::floor` and the rest.
- Don't rely on NaN or infinity: `-ffast-math` assumes they never occur. Clamp instead.
