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