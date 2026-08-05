# Competition SHADOW command replay integration

Date: 2026-08-05

## Scope and safety boundary

This document records the integration of the existing 64-entry replay window into the XIAO UART receive path for the Type 68--70 competition commands.  The only authoritative safety state remains `SafetyState` in `xiao-boat-control-integration`; it is converted at one call site to `competition_shadow::AuthoritativeSafety`.  `competition_shadow` does not own a second state machine.  `ACTUATOR_OUTPUT_ENABLE=0` and the competition controller physical gate remain false; no upload, COM/USB, SD, PCA9685, or VESC operation was performed.

## Formal receive pipeline

`linkRxService()` obtains a `boat::Frame` only after `boat::Decoder` has accepted COBS, frame version, frame length, and frame CRC.  For Types 68, 69, and 70 it calls `handleCompetitionCommand()`, which calls `CommandIngress::process()` before any control application.

1. Validate payload size, payload protocol version, canonical CRC, enum, finite values, and ranges.
2. Convert the validated payload to the canonical replay identity: protocol version, type, request ID, command sequence, source timestamp, payload length, and all command-specific fields.  The existing canonical CRC is the fingerprint; it was computed across every preceding wire field, including source timestamp and command-specific fields.
3. Inspect the single 64-entry `CommandReplayWindow`.
4. Only `NEW` reaches `Controller::setMode`, `setManual`, or `setHeading`; both accepted and state-rejected NEW results are stored.
5. Generate the existing 36-byte Type 71 ACK.  A duplicate ACK has disposition `Duplicate`, carries the original reason, and reuses the original applied time.

Malformed input never reaches or changes the window.  Conflict, stale, and ambiguous-half-range inputs are neither applied nor stored.  The half-range condition is a stale disposition with the explicit `AmbiguousSequence` reason.  A reset recreates the in-memory ingress/window and baseline, as intended.

## Wire compatibility

| Type | Payload | Size |
| ---: | --- | ---: |
| 68 | ControlModeCommand | 24 B |
| 69 | ManualCommand | 40 B |
| 70 | HeadingTarget | 28 B |
| 71 | ControlCommandAck | 36 B |

No Type 68--71 layout, CRC coverage, or size changed.

## Diagnostics

The regular XIAO serial diagnostics now emit `COMPETITION_CMD` with: new, applied, rejected, duplicate, conflict, stale, ambiguous, malformed, ACK, duplicate-reapply, replay-entry count, high-watermark, and physical-write counts.  Acceptance invariants are `duplicate_reapply=0` and `physical_writes=0`.

## Host evidence

`competition_command_ingress_host.cpp` uses `boat::encode` and `boat::Decoder` before calling the same `CommandIngress::process()` used by the XIAO firmware.  It passed Type 68/69/70 first application; duplicate ACK resend; rejected command retry without reevaluation; ID/sequence/type/payload conflicts; malformed protocol-version, payload-length, canonical-CRC, NaN, Inf, invalid-enum, COBS, and frame-CRC inputs; a valid frame after malformed input with the same identity; wrap; half-range rejection; and >64 entries.

The manual timing path was asserted as follows:

| Time | Input/result |
| ---: | --- |
| 0 ms | NEW manual accepted |
| 100/200/300/400 ms | exact duplicate; original applied time retained |
| 500.001 ms | `ManualTimeout` and `Disarm` request |

The test asserts one original application, no duplicate reapplication, and zero physical writes.

## Current limit

The integration in this commit covers Type 68--70, which are the new formal competition command layouts and the mandatory scope.  Legacy Type 32--38 control commands and Type 66 WaypointSet retain their pre-existing protocol-specific handlers and ACK formats.  They are not represented as Type 71 commands and therefore are not claimed to be covered by this Type 68--70 ingress test.