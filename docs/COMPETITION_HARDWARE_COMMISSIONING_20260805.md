# Competition hardware commissioning — 2026-08-05

## Scope and branch

- Branch: `feat/competition-integrated-shadow-20260805` (PR #19). No merge/rebase was performed.
- This change adds a separate, opt-in XIAO environment: `competition_hardware`.
- Existing `competition_shadow` remains output-disabled and was rebuilt successfully.

## Audited wiring

| Function | Hardware definition |
| --- | --- |
| PCA9685 | I2C `0x40`, 50 Hz |
| Left-front wing | PCA9685 CH0 |
| Right-front wing | PCA9685 CH1 |
| Rear yaw | PCA9685 CH2 |
| VESC receive at VESC | XIAO D8 / GPIO7 (XIAO RX) |
| VESC transmit from VESC | XIAO D9 / GPIO8 (XIAO TX) |

The source already used D8 as the XIAO VESC RX pin and D9 as TX. No pin was guessed or changed.

## Safety boundary and initial values

Only `competition_hardware` sets `COMPETITION_HARDWARE_ENABLE=1` and `ACTUATOR_OUTPUT_ENABLE=1`. It is mutually exclusive with `competition_shadow`, benchmark and replay modes.

- boot state: `DISARMED`
- initial control mode: manual
- PID gains: all P/I/D terms are zero in the hardware environment
- VESC: duty is zero until RUNNING, and zero on STOP/E-STOP/fault/timeout
- servo channels: neutral 1500 us; temporary narrow range 1480–1520 us
- servo rate: 100 us/s (maximum 2 us per 20 ms loop)
- reversals: all false until linkage direction is measured
- PCA write/readback failure while running: VESC zero, safe output, `FAULT`
- heartbeat/manual timeout and existing STOP/E-STOP paths remain active

`competition_shadow` remains physically zero. It cannot accidentally select the hardware output route.

## Control route

`CoreS3 Web -> Type 68/69/70 -> XIAO CommandIngress/replay -> authoritative SafetyState -> competition controller -> H/R/Y mixer -> ServoMapper -> PCA9685 / VESC`

The implemented mixer is the existing controller mapping:

- left-front = H + R
- right-front = H - R
- rear = Y

The conversion from BNO axes to body axes remains the source-defined identity transform and `kBnoMountValidated=false`. Therefore PID gains are zero and attitude/heading control must not be tuned or enabled until the physical body-axis test records the actual mapping. Manual, zero-propulsion servo commissioning is the first permitted physical stage.

## CoreS3 web controls

Existing `/competition` continues to issue acknowledged type 68/69/70 commands. The page also now exposes the existing safety wire commands through:

- `POST /api/competition/safety?action=arm`
- `POST /api/competition/safety?action=start`
- `POST /api/competition/safety?action=disarm`
- `POST /api/competition/safety?action=stop`
- `POST /api/competition/safety?action=estop`
- `POST /api/competition/safety?action=clear_estop`

ARM/START do not output until the XIAO has the dedicated hardware image, a valid heartbeat is present, and a fresh manual command exists. STOP/E-STOP use the pre-existing protocol types and remain unchanged in behavior.

## Validation completed

- `COMPETITION_ACTUATORS_HOST_PASS narrow_limits=ok rate_limit=ok reverse=ok`
- XIAO `competition_shadow`: PASS, RAM 211300 / 327680, Flash 586361 / 3342336
- XIAO `competition_hardware`: PASS, RAM 211316 / 327680, Flash 587489 / 3342336
- CoreS3 `competition_cores3_shadow`: PASS, RAM 175772 / 327680, Flash 1047661 / 6553600
- CoreS3 `competition_cores3_hardware`: PASS, RAM 175772 / 327680, Flash 1047677 / 6553600

## Not yet completed

No COM device was opened, no firmware was uploaded, and no PCA9685/VESC physical command has been sent in this change.

Before upload and physical output testing, identify COM4 and COM6 again and confirm that COM3 remains untouched. Before the first ARM operation, physically remove the propeller or secure it so a 1%/3% VESC test cannot cause injury. Servo direction/range and BNO body-axis mapping are unverified and must be logged during commissioning. NVS-persistent tuning storage is not implemented in this first safe-output increment; only the compile-time temporary commissioning values above are active.