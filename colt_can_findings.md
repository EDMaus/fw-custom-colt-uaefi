# Colt CZT CAN findings

This file tracks the tested CAN states so we do not have to rediscover the same IDs during SRS/ASC/body debugging.

## Current anchors

- `0x212` controls/affects ASC/slip state.
- `0x212` running payload `03 92 00 00 68 0F 00 00` makes ASC/slip go out.
- `0x212` running payload `03 A1 00 00 68 12 00 00` was the SRS-friendly side in earlier testing, but brought ASC/slip back.
- Switching `0x212` from `03 92 ... 0F` to `03 A1 ... 12` after a timer caused bad SRS/beep behavior. Do not use a timed `0x212` transition as the final fix.
- `0x308` controls/affects RPM/MIL bulb-check behavior.
- `0x308` stock key-on sequence observed:
  - `00 00 00 06 01 33 FF 00`
  - `00 00 00 04 01 33 FF 00`
  - `00 00 00 04 00 33 FF 00`
- Matching the `0x308` settle phase removed the repeated beep in the 2026-05-12 test, but SRS still starts blinking after the key-on self-check window.
- In the later "beep returned" test with the same firmware, the main difference from the prior no-beep test was `0x412` switching to/holding `34 00 05 D7 8C 4A 11 FF` after start. The no-beep test mostly stayed around `34 00 05 D7 8C 4A 01 FF`.
- `0x1E1` is present from another module as `81 00 00 00 00 00 00 00` while the firmware has also transmitted `00 00 00 00 00 00 00 00`.
- Making firmware `0x1E1` pre-run `81 ...` made behavior worse and brought more beeps/ASC issues. Keep firmware `0x1E1` clear (`00 ...`) unless a new log proves otherwise.
- `0x423` is not transmitted by current firmware. It is present from another module as `03 00 00 08 2E BC` during key-on/running in rusEFI tests.
- `0x443` is not transmitted by current firmware. It is present as `00 02 00 00 00 00`.
- A narrow isolation build after `701f20ea` transmits only OEM-like `0x412 = 58 00 05 D7 8C 5C 01 FF` at 100 ms. This is intended to isolate SRS/beep without reintroducing broad body-frame replay.

## Commit behavior notes

- `2bd8f4bd Restore Colt 0x1E1 clear state`
  - `0x1E1 = 00 ...`
  - `0x212 key-on = 05 66 00 00 68 EC 00 00`
  - `0x212 running = 03 A1 00 00 68 12 00 00`
  - `0x308 key-on = 00 00 00 04 00 33 FF 00`
  - This is the likely "SRS/piep good, ASC/slip bad" state.

- `26fd3233 Match Colt 0x212 OEM ASC state`
  - `0x212 key-on = 05 66 00 00 68 E9 00 00`
  - `0x212 running = 03 92 00 00 68 0F 00 00`
  - This is the ASC/slip-good side, but SRS/piep regressed.

- `701f20ea Match Colt 0x308 self-check settle phase`
  - Keeps ASC-good `0x212`.
  - Adds stock-like `0x308` key-on settle phase.
  - Test result: ASC out, repeated beep gone, SRS still solid for about 5 seconds then blinking.

## Current open problem

ASC and repeated beeps are improved with the current firmware, but SRS still blinks after the key-on self-check window and keeps blinking after start.

Next suspects should be key-on frames that change during the same window, not running-only frames:

- `0x408`
- `0x412`
- `0x416`
- possible interaction with duplicate `0x1E1`

Do not reintroduce broad body-frame replay without isolating the specific frame, because prior tests caused door indicator/HVAC/fan side effects.
