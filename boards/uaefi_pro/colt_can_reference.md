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
- The most critical IDs for A/C behavior are `0x443` and the compressor control output configuration.
- USB / TunerStudio connection can change observed bus behavior because the ECU becomes active and may emit extra internal traffic such as `0x190` and `0x191`.

## Last known observations

- OEM `KEY ON` uses noticeably different payloads than older custom uaEFI key-on baselines, especially on `0x408`, `0x412`, `0x416`, `0x423`, `0x308`, and `0x608`.
- With uaEFI powered by USB only, CAN transmission should stay quiet unless ignition is genuinely on.
- `0x190` and `0x191` are confirmed as built-in wideband module traffic and should not be mistaken for missing vehicle-network frames.
