# Competition actuator commissioning — 2026-08-05

This document records the second safety increment before any hardware upload.

## VESC command safety

- `target_duty` is the accepted normalized manual command, clamped to 0.03.
- `applied_duty` is the only duty sent to VESC in normal operation.
- The dedicated hardware profile services VESC duty at 50 Hz (`20 ms`).
- Normal rise and fall both take 500 ms from 0 to the 3% limit.
- STOP, E-STOP, heartbeat loss, controller fault, PCA failure, test timeout and explicit `safeOutputs()` call `stopImmediate()` and transmit duty zero immediately; they do not use the 500 ms fall ramp.
- The once-per-second `VESC_RAMP` serial line records target/applied duty, active ramp, VESC reported duty, ERPM, motor/input current, voltage, fault and XIAO safety state.
- VESC reported duty is telemetry returned by VESC and is distinct from the requested and applied commands.

## Servo commissioning defaults

- PCA9685 address: `0x40`, 50 Hz.
- CH0 left front, CH1 right front, CH2 rear yaw.
- Each channel starts at 1500 us and is limited to 1480–1520 us.
- Per-channel rate limit is 100 us/s. Reversal defaults to false.
- PID values are all zero for the hardware build. Body mounting remains unvalidated, so attitude/heading PID tuning is prohibited until the physical axis test is recorded.

## Required physical sequence (not performed)

1. Confirm clean branch and build results.
2. Identify only boat XIAO COM4 and boat CoreS3 COM6. Do not open, upload, reset or otherwise operate COM3.
3. Upload both dedicated hardware images only after the propeller is removed or mechanically secured.
4. Verify boot is DISARMED, VESC target/applied duty are zero, and servos are at neutral.
5. With PID and VESC disabled, test CH0, CH1 and CH2 one at a time; record neutral/min/max/reverse/slew and direction.
6. Verify H/R/Y mixer directions before setting control signs or PID values.
7. Then test VESC at 1%, then 3%, each at most one second, recording target/applied duty, VESC duty, ERPM, currents, voltage and faults.
8. Verify STOP, E-STOP, deadman, control-node reset and communication-node reset each bring duty to zero.

## Evidence

- Host: `COMPETITION_ACTUATORS_HOST_PASS narrow_limits=ok rate_limit=ok reverse=ok duty_ramp=ok`
- XIAO `competition_shadow`: PASS, RAM 211324/327680, Flash 586681/3342336
- XIAO `competition_hardware`: PASS, RAM 211348/327680, Flash 588237/3342336

No device port has been opened, no firmware has been uploaded and no physical output test has been run for this increment.
## Actual hardware commissioning — 2026-08-05 (partial)

### Device boundary

- Only the boat devices were used: COM4 = control XIAO ESP32S3 and COM6 = communication/logging CoreS3. COM3 was not opened, reset, uploaded, or otherwise operated.
- PnP identity observed before upload: both COM4 and COM6 reported USB VID:PID `303A:1001`; the operator-designated mapping above was used. XIAO MAC reported by esptool: `34:85:18:AB:FA:90`. CoreS3 MAC: `30:ED:A0:D4:BF:40`.

### Rebuild and upload evidence

- Rebuilt before upload: XIAO `competition_shadow` PASS (RAM 211324/327680, Flash 586681/3342336); XIAO `competition_hardware` PASS (RAM 211348/327680, Flash 588237/3342336); CoreS3 `competition_cores3_shadow` PASS (RAM 175772/327680, Flash 1047661/6553600); CoreS3 `competition_cores3_hardware` PASS (RAM 175772/327680, Flash 1047677/6553600).
- XIAO `competition_hardware` was uploaded to COM4 twice. Both uploads reported complete image hash verification.
- The first XIAO boot showed `FAULT` before any host heartbeat. This was traced to `linkHealth()` treating the initial zero heartbeat timestamp as a timeout. The fix faults only after at least one host heartbeat has been received; timeout after an established heartbeat remains a FAULT with immediate safe output. All four configurations were rebuilt after this change and the XIAO hardware image was re-uploaded to COM4 with hash verification.
- Initial CoreS3 COM6 upload using the esptool stub failed before any image write with `Unable to verify flash chip connection (No serial data received)`. The dedicated CoreS3 hardware environment now uses ROM `--no-stub`. Rebuild and COM6 upload then completed; bootloader, partition table, and application were written, and the 1,048,048-byte application image hash was verified.

### Safe-state observation

- COM4 read-only diagnostics after the fixed image: `state=DISARMED`, `bno_ready=1`, `physical_writes=0`, VESC `target=0.0000`, `applied=0.0000`, `active=0`, reported VESC duty `0.0000`, and no VESC fault. Observed supply voltage was about 9.70 V. This meets the XIAO boot safety check.
- No ARM, START, manual motion, servo command, PID command, VESC test, SD log, or Web control request was sent. Therefore no physical actuator motion has occurred in this commissioning step.
- CoreS3 COM6 read-only serial access immediately produced `USB_UART_CHIP_RESET` and then an I/O disconnect on two attempts. A small amount of pre-reset application output was seen, but not enough to prove firmware identity, SoftAP, SD initialization, or XIAO link state. CoreS3 startup validation is **unconfirmed**, not passed. This serial-access/reset behavior must be resolved or observed through a non-resetting method before ARM or physical actuator testing.

### Physical-test hold

- The requested one-channel sequence must not use the present RUNNING path as-is: it drives all PCA channels to their commanded neutral values, whereas the required procedure needs exclusive CH0/CH1/CH2 excitation. A dedicated, bounded single-channel commissioning command (others Full OFF, VESC zero, no PID) is required before the CH0 1500/1520/1500/1480/1500 observation sequence.
- Propeller removal/securement has not been confirmed. No VESC motion test may be performed until the operator explicitly confirms it.
- Next safe software action: add and host-test that exclusive single-channel commissioning command; then rebuild both hardware images, re-upload as necessary, re-confirm the CoreS3 startup state, and wait for the operator before every physical servo step.