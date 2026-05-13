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
- Making firmware `0x1E1` pre-run `81 ...` until engine start made behavior worse and brought more beeps/ASC issues.
- Clean stock log `Auto_Joris_KEYtoacc5sec_keytoON10sec_start_idle morethanminute.csv` shows the correct SRS self-check shape is timed, not running-dependent: `0x1E1 = 81 00 00 00 00 00 00 00` for about 6.7 seconds after key-on, then `00 00 00 00 00 00 00 00` before engine start.
- A firmware test matching this timed `0x1E1` self-check did not fix SRS: airbag stayed on, then started blinking where it should have gone out. Do not keep this as the base.
- Full stream comparison against the clean stock log shows the earliest ECU-frame mismatch at key-on is `0x212`: stock uses `05 37 00 00 68 DA 00 00`, while prior firmware used `05 66 00 00 68 E9 00 00`.
- `0x423` is not transmitted by current firmware. It is present from another module as `03 00 00 08 2E BC` during key-on/running in rusEFI tests.
- `0x443` is not transmitted by current firmware. It is present as `00 02 00 00 00 00`.
- A narrow isolation build after `701f20ea` transmitted only OEM-like `0x412 = 58 00 05 D7 8C 5C 01 FF` at 100 ms. Test result: SRS/beep stayed the same, ASC went out earlier. `0x412` alone is not the SRS/beep fix.
- The clean stock CZT uses very different `0x408`/`0x412`/`0x416` payload families from earlier logs, so those frames are likely body/option/checksum dependent and should not be blindly replayed.

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

- `cc22bb52 Revert isolated Colt 0x412 test`
  - Returns to no firmware `0x412` transmit.
  - Test evidence showed isolated `0x412` did not fix SRS/beep.

- `9d6e7472 Match Colt 0x1E1 SRS self-check timing`
  - Keeps ASC-good `0x212`.
  - Sends `0x1E1 = 81 ...` for 6.7 seconds after key-on, then clears to `00 ...`, matching the clean stock SRS self-check timing before start.
  - Test result: not fixed. SRS stayed on, then blinked when it should go out, and beep returned after start.

- Current key-on `0x212` test
  - Reverts firmware `0x1E1` to clear (`00 ...`) because the timed `0x1E1` test failed.
  - Keeps ASC-good running `0x212 = 03 92 00 00 68 0F 00 00`.
  - Changes key-on `0x212` to clean-stock `05 37 00 00 68 DA 00 00`.

## Current open problem

ASC and repeated beeps are improved with the current firmware, but SRS still blinks after the key-on self-check window and keeps blinking after start.

Next test should validate whether matching the timed `0x1E1` self-check state lets SRS finish before start. If not, next suspects should be key-on frames that change during the same window, not running-only frames:

- `0x408`
- `0x412`
- `0x416`
- possible interaction with duplicate `0x1E1`

Do not reintroduce broad body-frame replay without isolating the specific frame, because prior tests caused door indicator/HVAC/fan side effects.
