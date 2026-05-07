# Colt CZT CAN Reference

This file tracks the currently known Mitsubishi Colt CZT CAN IDs used by the
`uaefi_pro` board integration, based on OEM logs and ongoing reverse
engineering.

## Status meanings

- `High`: confirmed by logs and/or active use in firmware
- `Medium`: behavior is mostly understood, but byte-level meaning is still partial
- `Low`: seen on the bus, but meaning is still uncertain

## ID table

| ID | Direction | Meaning | Confidence | Notes |
| --- | --- | --- | --- | --- |
| `0x0C0` | TX | RPM / engine-speed cluster frame | High | Sent by uaEFI |
| `0x190` | Internal | Wideband module traffic | High | Comes from the ECU's built-in wideband module |
| `0x191` | Internal | Wideband module traffic | High | Comes from the ECU's built-in wideband module |
| `0x200` | RX | Body/input status, including brake | High | Used in firmware for brake state |
| `0x210` | TX | Throttle / engine-state frame | High | Sent by uaEFI |
| `0x212` | TX | Throttle / engine-state frame | High | Key-on baseline matters |
| `0x300` | RX | ASC / traction / stability status | Medium | Used in firmware for ASC intervention state |
| `0x308` | TX | Cluster engine info with RPM content | High | Tach/RPM-related |
| `0x312` | TX | Companion cluster / engine frame | Medium | OEM-stable, byte meaning still partial |
| `0x408` | TX | Key-state / ECU-state / body-state frame | High | Strongly correlated with fan/body behavior |
| `0x412` | TX/RX | Key-state / ECU-state / meter/body frame | High | OEM key-on payload differs from older custom payloads |
| `0x416` | TX | State-indicator frame | High | Distinguishes OFF / ACC / ON / running |
| `0x423` | TX | Body / light / door-state frame | High | Confirmed by logs and prior Colt notes |
| `0x443` | TX/RX | A/C request / A/C status frame | High | A/C request bit lives here |
| `0x584` | TX | Keepalive / ECU-present frame | High | Core cluster/body keepalive |
| `0x608` | TX | Extra engine/body state frame | Medium | Relevant at key-on, byte meaning still partial |
| `0x0EF50000` | RX | Unknown extended frame | Low | Seen during logging, not yet assigned |

## Current uaEFI usage

### Transmitted by uaEFI

- `0x0C0`
- `0x210`
- `0x212`
- `0x308`
- `0x312`
- `0x408`
- `0x412`
- `0x416`
- `0x423`
- `0x443`
- `0x584`
- `0x608`

### Received by uaEFI

- `0x200`
- `0x300`
- `0x412`
- `0x443`

## Practical notes

- The most critical IDs for fan / key-on behavior are `0x408`, `0x412`, `0x416`, and `0x608`.
- The most critical ID for door / lamp behavior is `0x423`.
- The most critical IDs for A/C and HVAC flap behavior are `0x443` and the compressor control output configuration.
- USB / TunerStudio connection can change observed bus behavior because the ECU becomes active and may emit extra internal traffic such as `0x190` and `0x191`.

## Last known observations

- OEM `KEY ON` uses noticeably different payloads than older custom uaEFI key-on baselines, especially on `0x408`, `0x412`, `0x416`, `0x423`, `0x308`, and `0x608`.
- With uaEFI powered by USB only, CAN transmission should stay quiet unless ignition is genuinely on.
- `0x190` and `0x191` are confirmed as built-in wideband module traffic and should not be mistaken for missing vehicle-network frames.

## OEM payload baselines by state

These are the most useful dominant payloads seen in the OEM captures. They are
intended as reverse-engineering references, not as a claim that every byte is
fully understood.

| ID | KEY OFF | KEY ACC | KEY ON | KEY ON / start |
| --- | --- | --- | --- | --- |
| `0x408` | `FF 00 FF FF 0C FF FF 00` | `FF 00 FF FF 0D FF FF 00` | `0F 00 63 FF FE C3 4F 00` or `0E 00 63 FF FE C3 4F 00` | `0E 00 65 64 FE C3 4F 00` at warm idle |
| `0x412` | `FF FF 05 D7 8C 00 01 FF` | `FF FF 05 D7 8C 00 01 FF` | `58 00 05 D7 8C 5C 01 FF` | `58 00 05 D7 8C 5C/5D 01 FF` at warm idle |
| `0x416` | `7C 00 00 00 00 00 00 00` | `7B 00 00 00 00 00 00 00` | `75 00 00 00 00 00 00 00` | mostly `8D ...`, `8E ...`, `8F ...` at warm idle |
| `0x423` | `00 00 02 08 2E BC` | `01 00 02 08 2E BC` | `03 00 00 09 2E BC` | `07 00 00 09 2E BC` at warm idle |
| `0x443` | not dominant in sample | not dominant in sample | `00 02 00 00 00 00` | `00 02 00 00 00 00` |
| `0x584` | not dominant in sample | `C0` | `C0` | `C0` |
| `0x608` | not dominant in sample | not dominant in sample | `34 00 18 C3 FF 00 00 00` | common warm-idle values include `66 00 18 C3 FF 00 65 00` and nearby dynamic variants |
| `0x308` | not dominant in sample | not dominant in sample | `00 00 00 04 00 33 FF 00` | RPM encoded in bytes 1-2, with byte 5 usually `0x53` at warm idle |
| `0x312` | not dominant in sample | not dominant in sample | `07 6F 07 6F 09 00 07 8E` | mostly `07 6F 07 6F 09 00 07 8E`, with cranking variants |
| `0x210` | not dominant in sample | not dominant in sample | `00 00 00 00 00 00 00 FF` | `00 00 00 40 00 00 00 FF` and `00 00 00 00 00 00 00 FF` |
| `0x212` | not dominant in sample | not dominant in sample | `05 66 00 00 68 E9 00 00` | `05 66 00 00 68 E9 00 00` plus several dynamic cranking values |

## Most useful state transitions

- `0x423` is a very strong body-state indicator:
  - `00` first byte at `KEY OFF`
  - `01` at `ACC`
  - `03` at `KEY ON`
- `0x416` is a very strong ECU-state indicator:
  - around `0x7C` at `KEY OFF`
  - around `0x7B` at `ACC`
  - around `0x75` at `KEY ON`
  - climbs into the high `0x8x` range at warm idle
- `0x408` and `0x412` shift from `FF`-heavy payloads into more structured `0E...` and `56...` payloads once the OEM ECU is truly alive at `KEY ON`.
- `0x443` should stay close to `00 02 00 00 00 00` while debugging body/HVAC behavior. Non-stock values such as `00 00 ...` and `00 03 ...` have been observed together with recirculation flap activity.
