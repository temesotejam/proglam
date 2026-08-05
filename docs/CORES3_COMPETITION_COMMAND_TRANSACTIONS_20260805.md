# CoreS3 competition command transaction integration

Date: 2026-08-05

## Scope

CoreS3 now originates Type 68 `ControlModeCommand`, Type 69 `ManualCommand`, and Type 70 `HeadingTarget` transactions and receives Type 71 `ControlCommandAck`. Type 66 and legacy START/STOP/E-STOP/Clear E-STOP remain on their existing paths and were not changed. The Type 68--71 field layout, size, protocol version, and canonical CRC ranges are unchanged.

No upload, COM/USB access, hardware test, microSD operation, or physical output operation was performed. The CoreS3 competition build continues with `ACTUATOR_OUTPUT_ENABLE=0`; `physical_writes` is always zero.

## Transaction model

`competition_command::Manager` has eight fixed `PendingCommand` slots. Each slot holds state, command type, request ID, command sequence, header sequence, source timestamp, serialized payload, pre-encoded COBS+CRC wire bytes, timing, transmission count, ACK result, reason, and XIAO applied time. There is no dynamic allocation.

States are `EMPTY`, `QUEUED`, `WAITING_ACK`, `ACK_APPLIED`, `ACK_REJECTED`, `ACK_DUPLICATE`, `PROTOCOL_ERROR`, and `TIMED_OUT`.

At creation, one payload and one `boat::encode` result are generated. A retry writes those same stored bytes; it never rebuilds a payload, timestamp, identifier, CRC, or header. Timeout is 100 ms and the maximum is three transmissions total (initial plus two retries). At 300 ms without an ACK the transaction becomes `TIMED_OUT`; it is never auto-reissued.

## Identity and restart behavior

CoreS3 reserves request-ID and command-sequence ranges of 256 values in NVS namespace `boatcmd` at boot. NVS is written once per reservation, not per command. A CoreS3 restart starts from the next reserved range and skips unused values, preventing reuse of an in-flight or recent XIAO replay identity. Both counters naturally use uint32_t wrap arithmetic. Header sequence remains separate and is retained in the stored retry frame.

## ACK and manual behavior

The CoreS3 UART receive task runs `Manager::handleAck` only after `boat::Decoder` validates COBS, frame version, frame length, and frame CRC. Type 71 then requires payload length, canonical CRC, disposition, type, request ID, and command sequence validation. Matching uses all three identifiers: request ID, command sequence, and command type.

An ACK is classified as applied, rejected, duplicate, conflict, stale, malformed, unmatched, or late. A duplicate ACK is a valid completion, not a new command. The original XIAO applied time is retained in status.

Manual web input is not refreshed in the background. A new HTTP manual request makes one new transaction only if no manual transaction is pending. The input becomes stale after 300 ms; no new command is produced from it, letting XIAO's existing 500 ms deadman timeout act. Retries remain the same transaction and thus XIAO treats them as duplicate without extending manual freshness.

## Web API

- `POST /api/competition/mode?mode=0..3`
- `POST /api/competition/manual?left_front_wing=-1..1&right_front_wing=-1..1&rear_yaw=-1..1&propulsion=0..1`
- `POST /api/competition/heading?target_yaw_rad=<finite>`
- `GET /api/competition/commands`
- Browser UI: `/competition`

Successful submit responses are HTTP 202 and report `status:"pending"`; they never claim that XIAO has applied the command. The status API exposes transactions and diagnostics. The once-per-second serial line `COMPETITION_CORE` exposes the same counters for capture logs. Input validation rejects missing/non-finite/out-of-range fields. Manual input while a manual transaction is pending returns explicit HTTP 409; fixed slots return explicit queue errors.

## Validation evidence

`core_command_transaction_host.cpp` uses CoreS3 `Manager`, real `boat::encode`/`boat::Decoder`, and XIAO `CommandIngress` in one path. It verifies Type 68/69/70 acknowledgement, byte-identical retry, two lost ACK retries, duplicate manual behavior and XIAO 500 ms timeout, finite retry timeout, request-ID/sequence/type-mismatched ACKs, late ACK, malformed ACK, queue-full behavior, wrap-near identifiers, and zero physical writes.

PlatformIO environment: `competition_cores3_shadow`.