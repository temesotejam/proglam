## 2026-07-27 -- provisional complete-path shadow system (not yet deployed)

- Added protocol Type 25 `ProvisionalSystem` to both firmware images. The communication node now receives the control ToF-frame centre zone and combines it with the existing primary-IMU snapshot, local secondary IMU, GNSS receiver and control EstimatedState in a 20 Hz shadow path.
- The secondary IMU is used only when both IMUs are fresh (<=250 ms), acceleration disagreement is <=3.0 m/s2 and gyro disagreement is <=0.25 rad/s. The result is explicitly provisional: a primary-estimate baseline with bounded dual-rate correction, not a validated hull-axis attitude or magnetic-yaw solution.
- GNSS requires a fresh valid fix plus latitude/longitude. ToF requires a fresh 0..4 m centre distance and an available attitude. `/api/provisional-system` reports each accept/reject reason and all input ages. It exposes a virtual DISARMED mixer: VESC duty and all three wing commands are 0 and no command is transmitted to the control XIAO.
- Type-25 records are emitted to the SD log at 10 Hz only while logging. Both default PlatformIO environments built successfully: communication RAM 190,376 bytes / Flash 887,509 bytes; control RAM 109,772 bytes / Flash 547,517 bytes. The control image was built only for protocol compatibility.
- Windows currently exposes only COM5, previously verified as the control XIAO. The communication XIAO (normally COM4, MAC `E0:72:A1:FC:08:D0`) is not connected, so the new communication image has not been uploaded or live-tested. Runtime validation remains pending.

## 2026-07-27 -- updated staged-estimation design accepted

- The new design requires a strict staged path: keep all physical outputs disabled, acquire reproducible calibration data first, validate it offline and with replay, then enable only shadow ESKF/LOS/mixing stages. It explicitly forbids treating a provisional IMU transform, a single gyro-difference sample, an unvalidated ToF geometry, or missing GNSS fix as calibration success.
- The existing dual-IMU/ToF/GNSS shadow system is therefore retained as Phase 0 transport and gate verification. It is not renamed or promoted to ESKF, stabilization, AUTO, servo geometry or VESC validation.
- Next implementation target is a unified calibration-run and replay contract with mode, sequence, timestamps, sensor status/accuracy, local/remote receive times, SD timing, health flags and validation result. Formal modes are restricted to DISARMED, CALIBRATION and REPLAY until the required calibration and safety gates pass.

## 2026-07-27 -- communication deployment and live verification

- Communication XIAO appeared on COM4 and the uploader identified MAC `E0:72:A1:FC:08:D0`. Firmware `0.3.8-provisional-shadow-system` was written successfully; every bootloader, partition, boot-app and application image segment passed esptool SHA verification.
- First live `/api/provisional-system` read revealed that an unconstrained long-term shadow rate correction could accumulate outside a plausible display range. This was never an actuator input, but it was corrected immediately by limiting each provisional dual-IMU correction component to +/-5 degrees. The fixed image rebuilt (RAM 190,376 bytes, Flash 887,545 bytes) and was uploaded to COM4 with all hashes verified.
- After reconnecting the PC to `XIAO-BOAT-TELEMETRY`, live API verification reported `attitude_available=true`, `dual_imu_accepted=true`, `tof_accepted=true`, `control_output_enabled=false`, `virtual_mode=DISARMED`, VESC=0 and all three virtual-wing commands=0. Live IMU ages were 0--11 ms, ToF age 10 ms / centre distance 1788 mm; the dual comparison reported accel delta 2.9076 m/s2 and gyro delta 0.0000 rad/s. GNSS had no valid fix and was correctly rejected as `fix_stale`.
- The control EstimatedState link was live but remains `DEGRADED` with `mount_validated=false`, and is not promoted to a control input. No physical actuator output was enabled or sent.

## 2026-07-27 -- CALIBRATION run contract deployed

- Added Type 26 `CalibrationMarker` and the `CALIBRATION` wrapper around the existing loss-aware P1 raw-IMU path. A selected calibration kind starts a `/CAL/RUNxxxx.BIN`, sends the existing P1 capture start to the control node, and records explicit start/stop markers with the session ID and kind. No new actuator command is sent.
- Added `/calibration`, `GET /api/calibration`, `POST /api/calibration/start?confirm=1&kind=...`, and `POST /api/calibration/stop`. Supported kinds are static-6face, X/Y/Z rotation, gyro bias, magnetic, time offset, ToF, servo geometry and VESC telemetry. CAL log files are available through the existing download endpoint.
- Communication and control firmware both built successfully. The communication image (RAM 190,424 bytes / Flash 891,989 bytes) was uploaded to COM4 / MAC `E0:72:A1:FC:08:D0` with all image hashes verified. Live `/api/calibration` returned inactive, SD errors 0, `control_output_enabled=false` and `virtual_mode=DISARMED`. No calibration measurement was started because the hardware is not yet fixed for a reproducible pose.

## 2026-07-26 -- BOAT24 staged timing diagnostics implemented; upload pending

- To isolate RUN0013's simultaneous BNO record gaps without adding serial output, the BOAT24 protocol now carries callback and BNO-event-queue-push timestamps in every BNO frame. The recorder retains frame creation, UART receive, common log-queue insertion, and SD-task dequeue timestamps in memory.
- New fixed-size TimingDiagnostic BIN records are emitted only for BNO frames whose recorder log-queue wait is at least 80,000 us. They contain the sensor timestamp; callback, BNO queue-push, frame, UART receive, log-queue, and SD-task times; and the start/end of the most recently completed SD write. TXT additionally reports timing_diagnostics and max_log_queue_wait_us.
- Both BOAT24 diagnostic images build successfully. Control: RAM 109,532 bytes / 33.4%, Flash 539,725 bytes / 16.1%. Communication: RAM 190,028 bytes / 58.0%, Flash 863,569 bytes / 25.8%. Control diagnostic BOAT24 was uploaded to COM4 / MAC 34:85:18:AB:FA:90 with every esptool segment hash-verified. Post-reset diagnostics show bno=1, increasing ToF frames, INA errors 0, and NAV errors/gaps 0. Communication diagnostic BOAT24 was then uploaded to COM5 / MAC E0:72:A1:FC:08:D0 with every esptool image segment hash-verified. Post-reset diagnostics repeatedly show SD=1, GNSS=1, BNO=1, logger drops 0 while idle, and increasing control-UART frames. Both diagnostic images are ready; the next RUN is pending.

## 2026-07-26 -- BOAT24 RUN0013: complete BNO acquisition, record-gap gate still fails

- RUN0013 completed normally: 4,533,378 bytes, 53,560 records, a structurally complete 59.999962 s phase, normal stop, logger queue drops 0, SD write errors 0, result CRC errors 0, and control-link drops 0. The control benchmark result is PASS with 1,800 ToF frames, zero incomplete ToF frames, 1,800 INA reads, I2C errors 0, and 60,007 ms duration.
- Communication-local BNO within the phase: accel 7,460 / 124.333 Hz / missing 0; gyro 5,992 / 99.867 Hz / missing 0; magnetic 1,498 / 24.967 Hz / missing 0. TXT counters show callback decode errors 0, BNO queue drops 0, and high-water 9 of 96.
- Control-over-UART BNO within the phase: accel 7,593 / 126.550 Hz / missing 0; gyro 6,001 / 100.017 Hz / missing 0; magnetic 1,500 / 25.000 Hz / missing 0. Control BENCH code 29 records 15,093 callbacks and 0 BNO queue drops; code 30 high-water 3 of 96 and decode errors 0; code 31 21,429 SH-2 service calls and 7,847 us maximum service; code 32 15,093 UART enqueue successes and 0 failures. This confirms the BOAT24 report-selection correction and the entire callback-to-UART path.
- RUN0013 nevertheless fails the explicit <=80 ms magnetic record-gap gate. The local/control magnetic maximum recorded gaps are 111.384/115.311 ms. Both magnetic streams have their largest gaps across the same 9.55--9.66 s interval, while their sequences remain continuous. This rules out BNO acquisition and control-UART loss; it is consistent with recorder-path scheduling/SD write latency (TXT maximum SD write 81.099 ms). Add/compare BNO callback, queue enqueue, BIN frame creation, and SD-write timing before another BOAT24.
- Decision: do not start BOAT23. RUN0013 establishes loss-free three-stream acquisition on both nodes, but not the requested logged-time continuity.

## 2026-07-26 -- BOAT24 control magnetic selection corrected; upload pending

- Corrected the control Reader report condition so BOAT24, as well as BOAT23, enables calibrated gyro, accelerometer, and calibrated magnetic field. BOAT24 now cannot fall through to the legacy gyro/game-rotation/accel branch.
- The corrected control BOAT24 build succeeded: RAM 109,532 bytes (33.4%), Flash 539,777 bytes (16.1%). Control USB then appeared as COM4. The corrected BOAT24 image was uploaded to MAC 34:85:18:AB:FA:90 with every esptool image segment hash-verified. Post-reset diagnostics repeatedly show bno=1, increasing ToF frames, INA errors 0, and NAV errors/gaps 0. Both nodes now run BOAT24; the repeat RUN has not started.

## 2026-07-26 -- BOAT24 RUN0012: control report selection defect found

- E:\BENCH\RUN0012.BIN/TXT completed normally: 4,043,121 bytes, 46,772 records, 60.050 s phase, normal stop, zero SD write errors, zero logger queue drops, zero result CRC errors, and zero control-link drops. The control benchmark result is PASS with 1,801 ToF frames, zero incomplete ToF frames, 1,801 INA reads, zero I2C errors, and 60,057 ms duration.
- Communication-node local BNO intake is loss-free in the phase: accel 7,470 / 124.396 Hz / missing 0; gyro 5,997 / 99.866 Hz / missing 0; magnetic 1,499 / 24.962 Hz / missing 0. TXT diagnostics also show callback decode errors 0, BNO queue drops 0, and high-water 9 of 96. However, logged maximum gaps are about 200 ms (magnetic 200.252 ms), exceeding the <=80 ms magnetic record-gap criterion despite no sequence loss.
- Control BNO callback path itself is healthy: BENCH code 29 reports 9,806 callbacks and 0 BNO queue drops; code 30 reports high-water 3 of 96 and 0 decode errors; code 31 reports 13,727 SH-2 service calls and 7,460 us maximum service; code 32 reports 9,806 UART enqueue successes and 0 failures. But the phase has accel 3,800 / 63.280 Hz, gyro 3,002 / 49.991 Hz, game rotation 3,003, and magnetic 0. bno::Reader::enableReports() tests only BOAT_EXPERIMENT == 23 for the magnetic config, so BOAT24 falls through to the legacy accel/gyro/game-rotation configuration. This is a firmware selection defect, not an acquisition queue loss.
- Decision: RUN0012 is a useful partial confirmation but fails the BOAT24 gate. Correct the report condition to include BOAT24, rebuild/upload the control node, then repeat BOAT24 before considering BOAT23.

## 2026-07-26 -- Control BNO callback queue implemented; BOAT24 screening awaits COM5

- Replaced the control node `bno::Reader::poll()` use of Adafruit `getSensorEvent()` with a direct SH-2 callback registered after each initialization/reset. The callback decodes every sensor event and copies it to a 96-slot FreeRTOS queue; the dedicated BNO task drains that queue and only then creates UART telemetry frames. The D3 ISR still performs no I2C.
- The BNO task remains core 0 priority 3. It services SH-2 while D3 remains low, bounded by eight service calls per pass. The reader records callback count by type, decode errors, BNO queue drops/high-water, service calls, and maximum service time. Control main records BNO-to-UART enqueue successes/failures; BENCH event codes 29--32 preserve these values in the shared BIN log.
- Added BOAT24 `bno_accel100_gyro100_mag20_int_60sec` to both projects. BOAT24 retains BOAT23 sensor/ToF/INA/UART settings but uses a 60 s phase solely as a screening gate.
- Build verification: control BOAT23 and BOAT24 both succeeded (BOAT24 RAM 109,532 bytes / 33.4%; Flash 539,781 bytes / 16.1%). Communication BOAT24 succeeded (RAM 185,836 bytes / 56.7%; Flash 862,809 bytes / 25.8%).
- Upload: control COM4 / MAC `34:85:18:AB:FA:90` received BOAT24, with esptool hash verification. Post-reset serial showed `bno=1`, ToF frames increasing, INA errors 0, and NAV CRC/gaps 0. Communication COM5 / MAC `E0:72:A1:FC:08:D0` then reappeared and received the BOAT24 image with hash verification. Its post-reset diagnostics repeatedly show `SD=1`, `GNSS=1`, `BNO=1`, logger queue drops 0 while idle, and increasing received control-UART frames. No screening RUN has started yet.
## 2026-07-26 -- BOAT23 RUN0011: communication BNO fixed; control BNO remains incomplete

- `E:\BENCH\RUN0011.BIN/TXT` was fully parsed: 130,637 records, `normal_stop=1`, completed benchmark, SD write errors 0, logger queue drops 0, log fault none, result CRC errors 0, I2C errors 0, ToF incomplete frames 0, and control-link drops 0. The control-side benchmark result reports 180.042 s, ToF 5,401 frames, INA 5,401 reads, and status PASS.
- Communication-node local BNO was evaluated within the 180.026 s BenchmarkStart-to-BenchmarkStop window. Accel: 22,388 / 124.376 Hz / sequence missing 0 / max gap 43.823 ms. Calibrated gyro: 17,982 / 99.901 Hz / missing 0 / max gap 48.858 ms. Calibrated magnetic field: 4,496 / 24.979 Hz / missing 0 / max gap 73.186 ms.
- The communication BNO intake diagnostics validate the new path: 49,938 callback events, decode errors 0, BNO event-queue drops 0, high-water 10 of 96, and ignored events 0. The small difference between callback totals and BIN totals is pre/post measurement boundary traffic; there are no sequence gaps inside the evaluated phase.
- Control-node telemetry was evaluated in its matching 180.035 s phase. Accel: 22,717 / 126.187 Hz logged, 69 missing (126.571 Hz source), max gap 20.581 ms. Gyro: 17,399 / 96.653 Hz logged, 606 missing (100.019 Hz source), max gap 22.799 ms. Calibrated magnetic field: 2,641 / 14.684 Hz logged, 1,857 missing (25.009 Hz source), max gap 404.473 ms.
- Decision: RUN0011 proves the communication-side SH-2 callback queue solves its local acceleration/gyro/magnetic acquisition loss and does not harm SD/UART/I2C health. It does not make BOAT23 a loss-free two-node result, because the control-side wrapper path remains the loss source. Copy the callback-queue design to the control BNO task before the next acceptance RUN.
## 2026-07-26 -- Communication BNO callback queue: build, upload, boot verified; measurement pending

- Root cause addressed on the communication node: Adafruit BNO08X `getSensorEvent()` stores decoded reports in one caller-provided object, so several callbacks handled by one `sh2_service()` can overwrite earlier reports before the task consumes them.
- Firmware `0.3.1-bno-callback-queue` installs its own SH-2 sensor callback after BNO initialization. Each successfully decoded report is copied into a 96-slot FreeRTOS queue; the `CommBno` task drains it into the existing BIN log. The D3 FALLING ISR remains notification-only and never accesses I2C.
- Local report configuration is now accelerometer 100 Hz, calibrated gyro 100 Hz, calibrated magnetic field 20 Hz. Game Rotation Vector is not enabled. `CommBno` priority changed from 1 to 3, above `ControlRx` priority 2. The TXT summary and `GET /api/sensors` now expose INT edges, callback event counts by type, decode errors, BNO event-queue drops/usage/high-water, service calls, and maximum service time.
- `bno_accel100_gyro100_mag20_int_3min` built successfully (RAM 185,836 bytes / 56.7%; Flash 862,809 bytes / 25.8%). COM5 (MAC `E0:72:A1:FC:08:D0`) upload succeeded with esptool hash verification. After reset, serial diagnostics repeatedly reported `SD=1`, `GNSS=1`, `BNO=1`.
- No post-change 3-minute RUN has been started. Do not infer loss-free magnetic acquisition from the build or boot result. The next RUN must evaluate the new BNO counters and all prior BOAT23 pass criteria. The control-side callback conversion remains deliberately unperformed in this change.
## 2026-07-25 -- BOAT23 RUN0010 analysis: logging fixed, BNO stream loss remains

- `E:\BENCH\RUN0010.TXT/BIN` completed normally: `normal_stop=1`, 90,246 parsed records, queue drops 0, SD write errors 0, log fault none, benchmark status pass, I2C errors 0, ToF incomplete frames 0, and control link drops 0.
- In the 179.994 s control-node benchmark window, BNO records were: accel 22,668 / 125.939 Hz / 92 sequence gaps / 21.150 ms maximum gap; gyro 17,357 / 96.431 Hz / 646 gaps / 25.484 ms; calibrated magnetic field 2,622 / 14.564 Hz / 1,879 gaps / 405.776 ms.
- The communication node's local BNO stream is also severely under-serviced after the new task split (accel 27.595 Hz, gyro 13.057 Hz, magnetic 1.522 Hz over the logged interval). The current `CommBno` priority 1 task is likely starved by the priority 2 UART task while it drains continuously. Do not accept RUN0010 as a loss-free BNO result; adjust task scheduling and repeat.

## 2026-07-25 -- Communication-side INT firmware uploaded

- Communication XIAO ESP32-S3 Sense was detected on COM5 (MAC E0:72:A1:FC:08:D0) and updated with `bno_accel100_gyro100_mag20_int_3min` including the `CommBno` INT-driven task.
- Esptool hash-verified every written image segment. No post-upload BOAT23 measurement has been started yet.

## 2026-07-25 -- Communication-side BNO INT task (build verified; hardware pending)

- Moved communication-side BNO08X acquisition off the Arduino loop into `CommBno` on core 0, priority 1. Its D3 falling-edge ISR only notifies the task; the task handles one ready event per pass and uses a 2 ms fallback for recovery/status handling.
- `ControlRx` remains on core 0 at priority 2, so UART control-frame decoding has precedence over the local BNO task. The loop remains responsible for GNSS parsing, command/benchmark state, SD logging, and the Web UI.
- `bno_accel100_gyro100_mag20_int_3min` compiled successfully for the communication node. This is an intake-scheduling change only; it does not yet prove that RUN0009's 1,414 SD queue drops are resolved.


## 2026-07-25 -- Added calibrated magnetic-field acquisition (unverified on hardware)

- BNO08X calibrated magnetic field (X/Y/Z in uT) was added to the control-node telemetry protocol and the communication-node log.
- The communication node now exposes `GET /api/sensors` and `http://192.168.4.1/sensors`, including acceleration (m/s2), gyro (rad/s), magnetic field (uT), magnetic validity, accuracy, and age.
- Both `bno_accel100_gyro100_mag20_int_3min` PlatformIO environments compiled successfully. Neither image has been uploaded or measured yet.
﻿# 作業ログ
## 2026-07-25 -- BOAT23 communication-node upload

- Communication/recording XIAO ESP32-S3 Sense was detected as COM5 (MAC E0:72:A1:FC:08:D0) and uploaded with `bno_accel100_gyro100_mag20_int_3min`.
- Esptool verified every written image segment by hash. The control node has not yet been updated, so no BOAT23 measurement has started.

## 2026-07-25 -- BOAT23 control-node upload

- Control XIAO ESP32-S3 was detected as COM4 (MAC 34:85:18:AB:FA:90) and uploaded with `bno_accel100_gyro100_mag20_int_3min`.
- Esptool verified every written image segment by hash. Both nodes are now on BOAT23 and waiting for the 3-minute DRY_RUN to start from the communication-node Web UI.
- No measurement has been started or evaluated yet.
## 2026-07-25 — VL53L5CX／ストロベリー・リナックス #15315 の仕様を確認
## 2026-07-25 -- BOAT23 RUN0009 analysis: failed because of logging drops

- `E:\BENCH\RUN0009.BIN/TXT` completed normally in 180.078 s with `normal_stop=1`, SD write errors 0, log fault none, control-link drops 0, I2C errors 0, ToF incomplete frames 0, and VESC errors 0.
- The communication-side logging queue dropped 1,414 frames. This violates the experiment's mandatory queue-drop=0 criterion, so RUN0009 is not a valid acceptance result.
- Within the 3-minute control-node measurement window: accelerometer 22,499 samples / 124.945 Hz / 269 sequence gaps; gyro 17,218 / 95.616 Hz / 793 gaps; calibrated magnetic field 2,616 / 14.541 Hz / 1,882 gaps. The requested magnetic rate was 20 Hz.
- Conclusion: DO NOT use this RUN as evidence that three streams were acquired without loss. Diagnose communication-node logging throughput and BNO report/queue load before rerunning BOAT23.


- ST公式UM2884/DS13754を確認した。VL53L5CXの設定上限は8x8で15 Hz、4x4で60 Hz、I2Cは400 kHz〜1 MHzである。全出力時のドライバ転送量は4x4/60 Hzで63,000 bytes/s、8x8/15 Hzで50,909 bytes/sとされる。
- ストロベリー・リナックス #15315「VL53L5X TOFレーザー測距センサモジュール」は、正確にはVL53L5CXV0GC/1を搭載する。同社資料にモジュール固有の低いフレームレート上限は示されず、4x4/8x8・最大4 mを案内している。モジュールのI2Cロジックは3.3 V/5 V対応で、1.8 Vロジックには対応しない。
- 判断: P3で得た約8.3 Hzはチップまたは同モジュールの公称上限ではない。4x4化後の最大読出し時間は約13.4 msであるのに出力周期が約120 msのため、まず現行ファームウェアのToFデータready待ち・タスク周期・キュー投入のスケジューリングを診断対象とする。共有I2Cの400 kHz→1 MHz化は、同居するINA226/PCA9685と配線を含む別の検証が必要であり、この時点では実施しない。

## 2026-07-24 — P3 `p3_tof_4x4_30` を解析、P3は候補なしで完了

- RUN0011、RUN0012、RUN0014を有効な3反復として解析した。各BINは142,174/141,643/146,643レコードを末尾まで構造エラーなしで復号し、正常停止、SD書込みエラー0、キュードロップ0、I2Cエラー0、リンクドロップ0だった。測定区間の両boot IDシーケンス集合は欠番・重複なしで、GNSS安定区間のNAV起点の結果欠落も0だった。
- BenchmarkReady は全RUNで共有I2C 400 kHz、ToF 16ゾーン（4x4）・30 Hz要求、flags=95を示した。ToF実測は各2,505/2,496/2,489フレーム、実効8.35/8.32/8.30 Hz、最大ToF間隔217.771/133.823/133.347 ms、最大ToF読出し13.447/13.442/13.442 msだった。最大I2C時間は937/942/938 µs、最小free heapは全RUN 267,608 bytesで低下なしだった。
- RUN0013はSD書込み失敗（uffer_full、書込みエラー1）で開始直後に中断した。3,657レコードまでは構造復号できたが末尾に65 bytesの不完全レコードがあり、BenchmarkStop/Resultがないため比較対象外とする。
- P3総合判定: 8x8/10は7.21〜7.23 Hz、8x8/15は7.23〜7.30 Hz、4x4/15は8.30〜8.33 Hz、4x4/30は8.30〜8.35 Hzである。全設定が各要求Hzの90%に未達で、最大間隔条件も満たさない。4x4化で読出し時間は約36.3 msから約13.4 msへ短縮した一方、出力周期は約120 msのままであり、ToF読出し時間以外のスケジューリングが制約になっている可能性が高い。P3の後段候補はなしとし、P4へ進む前に取得処理の診断・改修または要件変更の判断が必要である。

## 2026-07-24 — P3通信側 `p3_tof_4x4_30` を書込み

- 通信側XIAO ESP32-S3 SenseをCOM5（MAC E0:72:A1:FC:08:D0）として認識し、xiao-boat-telemetry-integration の p3_tof_4x4_30 を書き込んだ。書込みデータのハッシュ検証を含めて成功した。
- 制御側・通信側とも同名環境となった。P3の4x4/30 Hz比較は、Web UI上のready確認後にDRY_RUNで5分を3反復する。

## 2026-07-24 — P3制御側 `p3_tof_4x4_30` を書込み

- 制御側XIAOをCOM4（MAC 34:85:18:AB:FA:90）として認識し、xiao-boat-control-integration の p3_tof_4x4_30 を書き込んだ。書込みデータのハッシュ検証を含めて成功した。通信側も同名設定を書き込むまで測定は開始しない。

## 2026-07-24 — P3 `p3_tof_4x4_15` を解析、条件を不採用

- RUN0007、RUN0009、RUN0010を有効な3反復として解析した。各BINは146,901/141,787/147,282レコードを末尾まで構造エラーなしで復号し、正常停止、SD書込みエラー0、キュードロップ0、I2Cエラー0、リンクドロップ0だった。測定区間の両boot IDシーケンス集合は欠番・重複なしで、GNSS安定区間のNAV起点の結果欠落も0だった。
- BenchmarkReady は全RUNで共有I2C 400 kHz、ToF 16ゾーン（4x4）・15 Hz要求、flags=95を示した。ToF実測は各2,491/2,500/2,499フレーム、実効8.30/8.33/8.33 Hz、最大ToF間隔150.588/220.226/159.518 ms、最大ToF読出し13.449/13.441/13.438 msだった。最大I2C時間は941/942/944 µs、最小free heapは全RUN 267,608 bytesで低下なしだった。
- RUN0008はSD書込み失敗（uffer_full、書込みエラー1）で約221秒に中断した。101,123レコードまでは構造復号できたが末尾に67 bytesの不完全レコードがあり、BenchmarkStop/Resultがないため比較対象外とする。後続の有効3反復ではSDエラーが再現しなかった。
- 判定: 4x4/15 Hzは8x8条件よりToF実効Hzを約1 Hz改善したが、15 Hz要求の90%（13.5 Hz）に3反復すべて未達であり、最大間隔も15 Hz要求の2周期（133.3 ms）未満を満たさない。後段候補には採用しない。次は p3_tof_4x4_30 を400 kHz固定で5分・3反復する。

## 2026-07-24 — P3通信側 `p3_tof_4x4_15` を書込み

- 通信側XIAO ESP32-S3 SenseをCOM5（MAC E0:72:A1:FC:08:D0）として認識し、xiao-boat-telemetry-integration の p3_tof_4x4_15 を書き込んだ。ビルドと書込みデータのハッシュ検証を含めて成功した。
- 制御側・通信側とも同名環境となった。P3の4x4/15 Hz比較は、Web UI上のready確認後にDRY_RUNで5分を3反復する。

## 2026-07-24 — P3制御側 `p3_tof_4x4_15` を書込み

- 制御側XIAOをCOM4（MAC 34:85:18:AB:FA:90）として認識し、xiao-boat-control-integration の p3_tof_4x4_15 を書き込んだ。ビルド（RAM 33.2%、Flash 16.0%）と書込みデータのハッシュ検証を含めて成功した。
- 共有I2C 400 kHzのまま、ToF 4x4/15 Hz条件へ切り替わった。通信側も同名設定を書き込むまで測定は開始しない。

## 2026-07-24 — P3 `p3_tof_8x8_15` 3反復を解析、条件を不採用

- RUN0004（131,693件、10,893,667 bytes）、RUN0005（136,726件、11,246,782 bytes）、RUN0006（136,881件、11,266,388 bytes）は、いずれもBINを末尾まで構造エラーなしで復号した。正常停止、SD書込みエラー0、キュードロップ0、I2Cエラー0、リンクドロップ0で、測定区間の両boot IDシーケンス集合に欠番・重複はなかった。GNSS安定区間のNAV起点の結果欠落も全RUN 0だった。
- BenchmarkReady は全RUNで共有I2C 400 kHz、ToF 64ゾーン（8x8）・15 Hz要求、flags=95を確認した。ToF実測は各2,191/2,169/2,181フレーム、実効7.30/7.23/7.27 Hz、最大ToF間隔239.840/179.889/235.111 ms、最大ToF読出し36.275/36.276/36.272 msだった。
- INA freshは2,016/2,193（91.9%）、2,015/2,169（92.9%）、2,015/2,183（92.3%）。最大I2C時間は958/940/935 µs、最小free heapは全RUN 267,608 bytesで低下なしだった。
- 判定: 8x8/15 Hzは保存・UART・I2C完全性を満たすが、15 Hz要求の90%（13.5 Hz）に3反復すべて未達であり、最大間隔も15 Hz要求の2周期（133.3 ms）未満を満たさない。8x8/10 Hzと比べても実効ToF Hzの改善はないため後段候補に採用しない。次は p3_tof_4x4_15 を400 kHz固定で5分・3反復する。

## 2026-07-24 — P3通信側 `p3_tof_8x8_15` を書込み

- 通信側XIAO ESP32-S3 SenseをCOM5（MAC E0:72:A1:FC:08:D0）として認識し、xiao-boat-telemetry-integration の p3_tof_8x8_15 を書き込んだ。ビルド（RAM 56.5%、Flash 25.7%）と書込みデータのハッシュ検証を含めて成功した。
- 制御側・通信側とも同名環境となった。P3の8x8/15 Hz比較は、Web UI上のready確認後にDRY_RUNで5分を3反復する。

## 2026-07-24 — P3制御側 `p3_tof_8x8_15` を書込み

- 制御側XIAOをCOM4（MAC 34:85:18:AB:FA:90）として認識し、xiao-boat-control-integration の p3_tof_8x8_15 を書き込んだ。ビルド（RAM 33.2%、Flash 16.0%）と書込みデータのハッシュ検証を含めて成功した。
- 共有I2C 400 kHzのまま、ToF 8x8/15 Hz条件へ切り替わった。通信側も同名設定を書き込むまで測定は開始しない。

## 2026-07-24 — P3 `p3_tof_8x8_10` 反復2・3を解析、条件を不採用

- RUN0002（130,763件、10,775,912 bytes）とRUN0003（130,819件、10,775,595 bytes）は、ともにBINを末尾まで構造エラーなしで復号した。両RUNとも正常停止、SD書込みエラー0、キュードロップ0、I2Cエラー0、リンクドロップ0で、測定区間の両boot IDシーケンス集合も欠番・重複なしだった。GNSS安定区間では、NAV起点の結果欠落は0だった。
- ToF実測はRUN0001/0002/0003で各2,169/2,162/2,168フレーム、実効7.23/7.21/7.22 Hzだった。最大ToF間隔は190.792/174.154/235.382 ms、最大ToF読出しは36.278/36.281/36.271 ms。RUN0003は最大間隔も10 Hz要求の2周期（200 ms）を超えた。
- INA freshは2,015/2,193（91.9%）、2,015/2,187（92.1%）、2,016/2,181（92.4%）で、最大I2C時間は949/935/947 µs、最小free heapは全RUN 267,608 bytesで低下なしだった。
- 判定: 8x8/10 Hzは保存・UART・I2C完全性を満たすが、ToF実効Hzが要求10 Hzの90%（9 Hz）に3反復すべて未達である。P3の後段候補には採用しない。次は固定400 kHzのまま p3_tof_8x8_15 を両XIAOへ書き込み、同じ5分・3反復で比較する。

## 2026-07-24 — P3 `p3_tof_8x8_10` 反復1（RUN0001）を解析

- E:\BENCH\RUN0001.BIN/TXT は129,130レコード・10,713,444 bytesを末尾まで構造エラーなしで復号した。測定は300.034秒で正常停止し、SD書込みエラー0、キュードロップ0、I2Cエラー0、リンクドロップ0だった。
- BenchmarkReady は共有I2C 400 kHz、ToF 64ゾーン（8x8）・10 Hz要求、flags=95（DRY_RUN/PCA OFF/VESC 0/ToF・INA ready）を示した。結果はToF 2,169フレーム（7.23 Hz）、不完全0、最大ToF読出し36,278 µs、INA fresh 2,015/2,193（91.9%）、最大I2C 949 µs、ヒープ267,608 bytesで低下なしだった。
- GNSSはRUN内でNAV 3,057件・結果3,056件、安定区間ではNAV起点の対応欠落0だった。両boot IDのシーケンス値集合は測定区間で連続・重複なしで、同時送信された数フレームの受信順が微小に入れ替わったのみである。
- 判定: 保存・UART往復・I2C完全性は合格。ただしP3候補基準のToF実効Hzは10 Hz要求の90%（9 Hz）以上に達せず、8x8/10 Hzはこの反復では後段候補にできない。P3の反復1として残り2回を同条件で実施し、再現性を確認する。

## 2026-07-24 — P3制御側 `p3_tof_8x8_10` を書込み

- 制御側XIAOをCOM4（MAC 34:85:18:AB:FA:90）として認識し、xiao-boat-control-integration の p3_tof_8x8_10 を書き込んだ。書込みデータのハッシュ検証を含めて成功した。
- これにより両XIAOはP3の共有I2C 400 kHz・ToF 8x8/10 Hz設定となった。Web UI上でreadyを確認してから、DRY_RUNのまま5分測定を3反復する。

## 2026-07-24 — P3通信側 `p3_tof_8x8_10` を書込み

- 通信側XIAO ESP32-S3 SenseをCOM5（MAC E0:72:A1:FC:08:D0）として認識し、xiao-boat-telemetry-integration の p3_tof_8x8_10 を書き込んだ。ビルドと書込みは成功（RAM 56.5%、Flash 25.7%）。
- P3は共有I2C 400 kHz、ToF 8x8/10 Hzの比較設定である。制御側も同名環境を書き込むまで測定は開始しない。
## 2026-07-24 — P2 RUN0018/RUN0019/RUN0020を解析（100 kHz固定は不採用）

- RUN0018は `benchmark_outcome=completed`、正常停止、SD書込みエラー0、キュードロップ0、BIN構造エラーなしだった。一方、測定区間のGNSS_NAV 6,215件に対しGNSS_PROCESS_RESULTは4,084件で、2,132件が対応しない。制御側P2結果はPASSを返したが、100 kHz・ToF 8x8/10 Hzの測定フェーズでToF 0フレーム、INA fresh 1,618、I2Cエラー0、最大I2C時間2,798 µs、リンクドロップ0だった。測定フェーズのToF停止と大きなNAV結果欠落により、比較用RUNとして不合格とする。
- RUN0019とRUN0020はともに約10秒で `prepare_timeout` により異常停止した。両RUNの制御側から `BenchmarkReady` は届かず、開始前のBenchmarkPrepare後に通信側が10秒待って中断した。SD書込みエラー・キュードロップは0だが、測定に入っていないため比較には使えない。
- 判断: 100 kHz固定は、P2で求める初期化・ToF取得・GNSS往復を安定して満たさない。過去のRUN0001で確認した動的速度復帰失敗とも整合するため、共有I2Cの通常構成には採用しない。400 kHz固定を維持し、P2は失敗として閉じる。
- 次: P3は400 kHz固定で開始し、まず `p3_tof_8x8_10` を両XIAOへ書き込み、ToFの実測Hz・最大間隔・他センサ/UARTへの影響を5分×3反復で比較する。

## 2026-07-24 — P2 通信側を書込み（測定開始前）

- 通信側XIAO（COM5）へ `p2_i2c_100k` を書き込んだ。PlatformIOのビルド・フラッシュ書込み・ハッシュ検証は成功した（RAM 56.5%、Flash 25.7%）。
- 制御側・通信側とも起動から100 kHz固定のP2で揃った。P2の10分測定はまだ開始していない。次: 通信側SoftAP/Web UIでSD、GNSS、比較BNO、制御リンク、DRY_RUNを確認し、P2を開始する。

## 2026-07-24 — P2 制御側を書込み（通信側待ち）

- 制御側XIAO（COM4）へ `p2_i2c_100k` を書き込んだ。起動から共有I2Cを100 kHz固定とするP2比較ファームウェアであり、測定途中の速度変更は行わない。PlatformIOのビルド・フラッシュ書込み・ハッシュ検証は成功した（RAM 33.2%、Flash 16.0%）。
- 通信側はまだP1のためP2測定は未開始。次: 通信側XIAOを接続し、同名の `p2_i2c_100k` を書き込む。

## 2026-07-24 — P1 RUN0016/RUN0017を解析（400 kHz基準値を確定）

- `RUN0016.TXT` / `RUN0017.TXT` はともに `benchmark_outcome=completed`、`normal_stop=1`、SD書込みエラー0、キュードロップ0、ログ異常なし、`failed_phases=0` を記録した。BINはそれぞれ605.580秒・261,424レコード、605.654秒・271,920レコードで末尾まで構造エラーなく復号できた。
- RUN0016 / RUN0017の両Boot IDは、連番欠落0・重複0。RUN0016のNAV 240〜6294と結果239〜6294は開始境界の結果239だけが対応外、RUN0017のNAV219〜6275と結果219〜6274は停止境界のNAV6275だけが対応外であり、共通する安定区間のGNSS往復は完全だった。
- P1結果は両RUNともPASS。RUN0016 / RUN0017はToF 4,349 / 4,369フレーム（未完了0）、INA fresh 4,030 / 4,030、I2Cエラー0、最大I2C時間944 / 943 µs、最大ToF読取り36,279 / 36,283 µs、リンクドロップ0、最小free heap 267,608 bytesだった。
- 送信時刻に基づく3反復の実測Hzは、BNO Accel=176.743/194.219/205.467、Gyro=101.064/108.956/112.887、Rotation=33.882/34.412/35.311、ToF=7.214/7.248/7.281、INA=7.300/7.320/7.392、GNSS_NAV=10.000/10.000/10.000、GNSS_RESULT=10.000/10.001/10.000。ToFは10 Hz要求に未達だが、400 kHz固定構成の再現可能な基準値として確定する。
- 判定: P1を完了。SD・UART・安全固定条件・メモリ・GNSS往復の完全性は3反復で確認した。次は電源再投入またはリセット後、100 kHz固定のP2を3反復し、測定途中のI2C速度変更は行わない。

## 2026-07-24 — P1 RUN0015を解析（第1反復は完全性確認）

- `E:\BENCH\RUN0015.TXT` は `benchmark_outcome=completed`、`normal_stop=1`、`records=259831`、`queue_drops=0`、`sd_write_errors=0`、`log_fault=none`、`failed_phases=0` を記録した。
- `RUN0015.BIN`（21,388,242 bytes）を605.546秒・259,831レコードとして末尾まで復号した。構造エラーなし。通信側Boot IDは127,366件、制御側Boot IDは132,465件で、両方ともグローバル連番の欠落0・重複0だった。RUN0014の採番重複は再現しなかった。
- TXTの測定区間NAV件数は6057/6056だが、BINではNAV 1453〜7508、結果1452〜7507であり、最初の結果1件と最後のNAV 1件の開始・停止境界差である。共通する1453〜7507は6055件すべて対応し、安定区間の実欠落ではない。
- 制御側P1結果はPASS。60万048 ms中にToF 4,329フレーム（未完了0）、INA 4,381読取り（fresh 4,029、duplicate 352）、I2Cエラー0、最大I2C時間942 µs、最大ToF読取り36,280 µs、リンクドロップ0、最小free heap 267,608 bytesだった。
- 送信時刻での実測は、BNO Accel/Gyro/Rotation = 176.743/101.064/33.882 Hz、ToF=7.214 Hz（p95間隔146.569 ms、最大237.229 ms）、INA=7.300 Hz、GNSS_NAV=10.000 Hz（p95 100.464 ms、最大105.557 ms）、GNSS_RESULT=10.000 Hz（p95 144.998 ms、最大231.055 ms）。ToFは10 Hz要求に未達であり、P1の基準値として記録し、P3の設定比較で改善可否を評価する。
- 判定: RUN0015はP1第1反復としてログ完全性・UART連番一意性・GNSS往復完全性を満たす。P1全体は同条件の残り2反復を待つ。

## 2026-07-24 — 連番競合修正版を制御側P1へ書込み

- 制御側XIAOをCOM4として認識し、`p1_stability_400k` の連番採番排他化修正版をビルド・書込み・ハッシュ検証した。成功（RAM 33.2%、Flash 16.0%）。
- 通信側は既存の `p1_stability_400k` のままでプロトコル互換である。次: P1を最初から再実行し、BINで制御側Boot IDの連番重複が0であることを確認する。

## 2026-07-24 — 制御側グローバル連番の競合を修正（実機書込み待ち）

- RUN0014の制御側Boot IDで発生した連番重複2件に対し、`linkSend()` の `++linkSeq` を `linkMux` のクリティカルセクション内で行う `nextLinkSequence()` へ置換した。Heartbeatペイロードが参照する現在値も同じロックで読むようにした。
- 制御側 `p1_stability_400k` をビルド成功した（RAM 33.2%、Flash 16.0%）。この修正は制御側のUARTヘッダー採番だけであり、通信側P1の再書込みは不要。
- 制御側XIAOは現時点でCOMポートに検出されず、修正版の実機書込み・P1再試験は未実施。次: 制御側XIAOを接続し、修正版をCOM4へ書き込んでP1を再実行する。

## 2026-07-24 — P1 RUN0014を解析（連番重複のため基準RUNは未合格）

- `E:\BENCH\RUN0014.TXT` は `experiment=P1_stability_400k`、`benchmark_outcome=completed`、`normal_stop=1`、`records=255310`、`queue_drops=0`、`sd_write_errors=0`、`log_fault=none`、`failed_phases=0` を記録した。測定区間のGNSS_NAV／GNSS_PROCESS_RESULTはそれぞれ6057/6058件で、往復欠落はない。
- `RUN0014.BIN`（21,069,116 bytes）を605.679秒・255,310レコードとして末尾まで復号した。構造エラーなし。通信側Boot IDは127,676件・連番欠落0・重複0、制御側Boot IDは127,634件・連番欠落0・重複2だった。重複はシーケンス137606（HeartbeatとToF）および150382（HeartbeatとBNO Accel）で、同じ送信時刻にも発生している。
- 制御側P1結果はPASS、DRY_RUN／PCA9685全OFF／VESC Duty 0／ToF準備済み／INA準備済みを確認。60万122 ms中にToF 4,320フレーム（未完了0）、INA 4,360読取り（fresh 4,030、duplicate 330）、I2Cエラー0、最大I2C時間943 µs、最大ToF読取り36,282 µs、リンクドロップ0、最小free heap 267,608 bytesだった。
- 送信時刻での実測は、BNO Accel/Gyro/Rotation = 149.798/84.241/28.193 Hz、ToF=7.196 Hz（p95間隔147.602 ms、最大186.409 ms）、INA=7.266 Hz、GNSS_NAV=10.000 Hz（p95 100.467 ms、最大103.708 ms）、GNSS_RESULT=10.001 Hz（p95 145.861 ms、最大222.042 ms）。ToFは10 Hz要求に未達であり、以後の設定選定の基準値として扱う。
- 判断: SD、GNSS往復、I2Cエラー、ヒープは良好だが、グローバル連番の一意性が満たされない。`linkSend()` がメインループとHeartbeat送信タスクから並行して呼ばれ、非同期の `++linkSeq` が競合することが原因候補である。採番を排他化して再書込み・再試験するまで、このRUNをP1の基準値として合格扱いにしない。

## 2026-07-24 — P1 通信側を書込み（測定開始前）

- 通信側XIAO（COM5）へ `xiao-boat-telemetry-integration` の `p1_stability_400k` を書き込んだ。PlatformIOのビルド・フラッシュ書込み・ハッシュ検証は成功した（RAM 56.5%、Flash 25.7%）。
- 制御側（COM4）・通信側（COM5）とも同名のP1固定条件ファームウェアで揃った。P1の10分測定はまだ開始していない。次: 通信側SoftAP/Web UIでSD、GNSS、比較BNO、制御リンク、DRY_RUNを確認し、P1を開始する。

## 2026-07-24 — P1 制御側を書込み（通信側待ち）

- E-STOPラッチをP1用ファームウェアの書込み時リセットで明示的に解除し、制御側XIAO（COM4）へ `xiao-boat-control-integration` の `p1_stability_400k` を書き込んだ。PlatformIOのビルド・フラッシュ書込み・ハッシュ検証は成功した（RAM 33.2%、Flash 16.0%）。
- P1は制御側・通信側が同名環境であることが開始条件である。通信側はP0のままのため、P1測定は未開始。次: 通信側XIAOを接続し、`xiao-boat-telemetry-integration` の `p1_stability_400k` を書き込む。

## 2026-07-24 — E-STOPラッチを制御側で直接確認（P0完了）

- Web UIからE-STOPを送信した直後、制御側COM4のシリアルに `STATE E_STOP` が出力された。その後も `DIAG state=E_STOP dry=1` が継続し、BNO08X正常、ToFフレーム増加、UART NAVの受信数と処理数の一致、NAV連番欠落0を確認した。
- 制御側のE-STOP処理は、安全出力（VESC Duty 0、PCA9685全OFF）を実行し、状態を `E_STOP` にラッチしてからACKを送信キューへ登録する。RUN0011ではログを先に閉じるためACKフレームはBINに残らないが、今回の直接状態確認により安全操作の受理・ラッチを確認した。
- 判定: P0のWeb UI、SD、GNSS、Heartbeat、DRY_RUN、STOP、E-STOP、BIN/TXT回収の経路を確認できたため、P0を完了とする。P1開始前には、E-STOPを再起動または`clear_estop`で明示的に解除する。

## 2026-07-24 — RUN0011のE-STOP操作を解析（ラッチ直接確認待ち）

- `E:\BENCH\RUN0011.TXT` は `benchmark_outcome=emergency_stop`、`normal_stop=0`、`log_fault=emergency_stop` を記録した。これはE-STOPで意図的に中断したための値であり、SD書込み異常ではない。SD書込みエラー0、キュードロップ0、測定区間のGNSS_NAV／GNSS_PROCESS_RESULTは160件ずつで一致した。
- `RUN0011.BIN`（542,428 bytes）は15.846秒・6,550レコードを末尾まで復号でき、構造エラーなし。両Boot IDのログ内シーケンスは制御側3,338件・通信側3,212件とも欠落0、重複0だった。
- ログには開始前STOPとそのACK（制御側DISARMED、DRY_RUN=1）、およびE-STOP起因の `BenchmarkEvent`（code=9、status=Aborted）がある。E-STOPフレームとACK自体は存在しない。通信側実装ではE-STOP要求で先にログを閉じてから制御側へ送るため、このBINだけでは制御側がE_STOPへラッチした証拠にはならない。
- COM4で制御側シリアル診断を試みた時点ではCOMポートが検出されなかった。再接続後の診断では `DIAG state=FAULT` であり、BNO08X正常、ToFフレーム増加、UART NAV連番欠落0だったが、`E_STOP` は確認できなかった。次: COM4で診断を見ながらWeb UIからE-STOPを再度送信し、`STATE E_STOP` または `DIAG state=E_STOP` を直接確認する。確認後、再起動または`clear_estop`でDISARMEDへ戻す。

## 2026-07-24 — P0 自動測定 RUN0009を解析（E-STOP確認待ち）

- `E:\BENCH\RUN0009.TXT` は `experiment=P0_bringup_400k`、`benchmark_outcome=completed`、`normal_stop=1`、`records=27046`、`queue_drops=0`、`sd_write_errors=0`、`log_fault=none`、`failed_phases=0` を記録した。測定区間のGNSS_NAVとGNSS_PROCESS_RESULTは658件ずつで一致した。
- `RUN0009.BIN`（2,237,288 bytes）を末尾まで復号した。65.722秒・27,046レコードで構造エラーはなく、両Boot IDのログ内シーケンスも制御側13,856件・通信側13,190件とも欠落0、重複0だった。
- 制御側の60秒P0結果はPASS。`DRY_RUN`、PCA9685全OFF、VESC Duty 0、ToF準備済み、INA準備済み、推定I2C量の各フラグを確認した。ToF 433フレーム、未完了0、INA 440読取り（fresh 402、duplicate 38）、I2Cエラー0、最大I2C時間930 µs、最大ToF読取り36,271 µs、リンクドロップ0、最小free heap 267,608 bytesだった。
- 安全操作は開始前STOPと、そのACK（制御側DISARMED、DRY_RUN=1）を記録した。一方、E-STOPフレーム／ACKはRUN0009に存在しないため、`EXPERIMENT_PLAN.md` のP0安全条件にあるE-STOP受理・ラッチは未検証である。
- 次: 記録停止中にWeb UIからE-STOPを一度送信し、制御側がE_STOPへ遷移してACKを返すことを確認する。その後、必要なら制御側を再起動または`clear_estop`でDISARMEDへ戻してP1の書込み・10分RUNへ進む。

## 2026-07-24 — P0 制御側を書込み（全体確認は継続中）

- 制御側XIAOを COM4（USB Serial Device、VID:303A/PID:1001）として認識し、`xiao-boat-control-integration` の `p0_bringup_400k` を書き込んだ。PlatformIOのビルド・フラッシュ書込み・ハッシュ検証は成功した。
- 起動診断では `dry=1`、BNO08X初期化成功、ToFフレーム数増加、INAエラー0、UART NAVの受信数と処理数の一致、およびNAV連番欠落0を観測した。
- 同じ診断では測定未開始の `bench=0` と `state=FAULT` を観測した。原因は未確認であり、P0の全体成立、SoftAP/Web UI、STOP/E-STOP、BENCHログの合格判定は未検証のままとする。
- 次: 通信側P0と接続した状態で、`FAULT` の理由を確認し、SoftAP/Web UIからP0の開始・STOP/E-STOP・ログ回収を実施する。

## 2026-07-23 — 固定構成RUNによる実機統合計測計画を決定

- `D:\BENCH\RUN0001.BIN` / `RUN0001.TXT`を解析した。55,360,811 bytes・663,027レコードを末尾まで復号でき、SD書込みエラー0、キュードロップ0であった。一方、周辺I2Cを100 kHzで測定した後に400 kHzへ戻すフェーズで制御側の`BenchmarkReady`が10秒以内に届かず、`prepare_timeout`で停止した。
- 判断: 共有I2Cの速度を通常運用中に切り替えない。周辺I2Cは400 kHz固定、BNO08X専用I2Cは100 kHz固定とし、100 kHz比較は起動時から100 kHzに固定した別RUNで行う。ToF設定、INA設定、UART負荷も同じRUN内では変更しない。
- 計画: `EXPERIMENT_PLAN.md`を追加し、P0（診断版の成立確認）、P1（400 kHz基準）、P2（100 kHz固定比較）、P3（ToF設定）、P4（INA設定）、P5（UART負荷）、P6（選定構成統合）、P7（長時間安定性）の順序と判定条件を定義した。
- 検証: 本項は計画と既存RUNの解析結果のみ。診断ブランチの最新コミットを両XIAOへ書込む実機検証は未実施である。
- 次: 診断ブランチの同一コミットを両XIAOでビルド・書込みし、P0を実施する。

## 2026-07-20 — RUN0018のSD書込み異常を安全停止へ変更（未実機書込み）

- `RUN0018.TXT` の `sd_write_errors=991` は、確認済みの `RUN0012.TXT`、`RUN0014.TXT`、`RUN0016.TXT` がすべて0だったのに対し、新たに発生した異常である。SD空き容量は約15 GBあり、容量不足は確認されなかった。
- 原因として、SDの一度の書込み失敗後に内部8192 byteバッファを満杯のまま残すため、同じ失敗書込みを繰り返してエラー数だけが増える実装を確認した。
- 対策: 書込み・flush失敗時にバッファとキューを破棄してログを異常停止し、画面/APIの `log fault` に理由を出す。異常停止したRUNには正常終了TXTを作らない。
- 併せて、`POST /api/log/start` は `confirm=1` を必須にした。古いブラウザタブや自動再試行が確認なしに新しいRUNを作ることを防ぐ。新しい画面でも確認ダイアログを表示する。
- 次: ビルド・通信側XIAOへの書込み後、開始しない状態でRUNが増えないこと、短時間記録の停止でTXTの `sd_write_errors=0` となることを確認する。

## 2026-07-20 — SDの余分なRUNファイル生成を停止

- 原因: 通信側`setup()`が起動ごとに`startLog()`を呼び、USB接続・リセット・書込みごとに空に近い`RUNxxxx.BIN`を作成していた。
- 対策: 起動時の自動`startLog()`を削除。Web UIの「記録を開始」を押した時だけBINを作成し、「記録を停止」時だけ同番号のTXT概要を作成する。
- `0.1.1-manual-log`として通信側をビルド成功。XIAOが未接続のため実機書込みは保留。

## 2026-07-20 — UI版の実機確認とGNSS送信の排他強化

- `RUN0016.BIN`: 232.897秒、102,858レコード。GNSS_NAV 2,329件、9.999 Hz、平均100.014 ms。SD queue drop=0、write error=0、TXT概要正常作成。STOP/E-STOPは2件ともACK accepted、DRY_RUN=1。
- 一方で制御側のGNSS_PROCESS_RESULTにsequence gap=1を検出。UI更新の影響ではなく、通信側UART送信mutexを20 msで取得できない場合にGNSS_NAVをスキップし得る実装が原因候補。
- `sendControl`を成功／失敗を返す関数へ変更し、GNSS_NAVはUART mutexを取得するまで待って全バイトを書けた場合だけSDの送信記録へ残すよう強化。ビルド成功、実機書込みと再試験はXIAO再接続後。

## 2026-07-20 — Web UIの操作結果を明確化

- Web UIを日本語の状態中心画面へ変更。SD、制御リンク、DRY_RUNを色付き表示し、記録状態・GNSS・往復結果・RTT・ACK・SDエラーを分けて表示する。
- 記録開始／停止は、押下後の送信要求と反映済み（停止時はTXT概要作成）を表示。STOP/E-STOPは送信要求後に制御側ACKを待ち、ACK後の制御側状態まで表示する。操作中は二重押下を防止。
- 通信側UI版のPlatformIOビルド成功。実機書込みはXIAOが未接続のため保留。

## 2026-07-20 — 10 Hz修正版とSTOP/E-STOPの実機成功（RUN0014）

- `RUN0014.BIN`: 212.169秒、93,109レコード、末尾まで完全に解析。通信側・制御側ともboot内の連番欠落0。
- GNSS_NAVは2,122件、実測9.999 Hz、平均100.013 ms。独立`GnssNavTx`タスクによる10 Hz要求を満たした。
- GNSS_PROCESS_RESULTは2,124件、result valid=2,121、duplicate=0、control側sequence gap=0、payload bad=0。記録境界に由来する対応外3件と、停止直前の未返信1件を除いて往復対応した。
- command ID 1 STOP、2 E-STOP、3 STOPの3件はすべてACK disposition=accepted。STOP後はDISARMED、E-STOP後およびその後のSTOPはE_STOPを維持。ACKはすべてDRY_RUN=1。
- `RUN0014.TXT`: normal_stop=1、queue_drops=0、sd_write_errors=0、result_bad_crc=0、last_rtt_us=18,785、command_ack_rx=3。

## 2026-07-20 — RUN0012の10 Hz未達を修正

- RUN0012のGNSS_NAVは平均168.399 ms（5.94 Hz）、p50=166.330 ms、p95=201.020 msであり、10 Hz要求を満たしていなかった。CRCや連番の問題ではなく、通信側通常ループの遅延が原因。
- 通信側のGNSS_NAV送信を通常ループからFreeRTOSの`GnssNavTx`タスクへ移した。`vTaskDelayUntil`で100 ms周期を維持し、UART送信はmutexでHeartbeat・時刻同期・コマンド送信と直列化した。
- 修正版の通信側ビルドは成功。実機書込みと10 Hz再確認はXIAOが再接続された後に実施する。

## 2026-07-20 — GNSS 10 Hz往復の実機成功（RUN0012）

- `RUN0012.BIN`: 770.491秒、333,693レコード。末尾まで完全に解析でき、3つのboot IDはいずれも内部連番欠落0。
- 通信側は`GNSS_NAV`を4,576件（約5.94 Hz）送信し、GNSS fix有効は4,567件。比較BNO・GNSS RAW/FIX/STATUS・Heartbeat・時刻同期requestもSDへ保存された。
- 新制御側の起動後、`GNSS_PROCESS_RESULT`を4,144件受信。4,144件すべてresult valid、payload bad=0、duplicate=0、sequence gap=0。結果には対応するGNSS_NAVが必ず存在した。
- GNSS_NAVと結果の差432件は、通信側が先に起動してから制御側が新ファームウェアで起動するまでの約43秒間に送られた分である。通信成立後の取りこぼしではない。
- `RUN0012.TXT`: normal_stop=1、queue_drops=0、sd_write_errors=0、result_bad_crc=0。通常停止時のTXT概要作成も確認。

## 2026-07-20 — GNSS往復・DRY_RUN統合を実装

- 通信側: GNSSを正規化した`GNSS_NAV`を10 Hzで制御側へ送信。制御側からの`GNSS_PROCESS_RESULT`、`COMMAND_ACK`、Heartbeat、時刻同期replyを受信・SD保存・`/api/link`とWeb画面へ表示する経路を追加。
- 制御側: GNSS_NAVのpayload CRC／連番／有効性を検証し、原点からの北東位置を含む結果を返信。100 ms Heartbeat、500 msリンク喪失FAULT、STOP/E-STOP ACKを追加。
- 安全: `kDryRunActuators=true`を既定にし、非ゼロVESC dutyとサーボパルスを抑止。安全停止時のVESC duty 0とPCA全OFFのみ許可。
- 検証: 通信側・制御側ともPlatformIOビルド成功。実機書込み／往復試験は両XIAOが未接続のため未実施。

## 2026-07-20 — 二台UART統合の実機記録確認

- `D:\BOATLOG\RUN0010.BIN` を解析。289.634秒、117,550レコードで末尾まで完全に読めた。
- 通信側 boot ID `0x09B7BD81`: 比較BNO accel/gyro/quat = 30,574/16,124/5,572件、GNSS raw/fix/status = 7,011/2,030/277件。最終GNSSは有効フラグ `0x7FF`、fix type 3、HDOP 0.79、衛星数41、GNSS checksum error 0、GNSS log drop 0。
- 制御側 boot ID `0x740443BC`: BNO accel/gyro/quat = 27,511/16,498/5,623件、ToF/INA/VESC = 2,101/2,114/2,115件。これはUARTを通じて通信側SDへ到達した。
- 両boot IDともログ上の連番欠落は0。今回の統合範囲では、制御側送信→UART→通信側受信/キュー→microSD保存が取りこぼしなく動いた証拠である。STOP/E-STOPの実機通過だけは未確認。

## 2026-07-20 — 通信側の全体縦切りを実装・書込み

- 新規: `xiao-boat-telemetry-integration` を作成。GNSS、比較BNO08X、制御側UART、microSD、SoftAP/Web UI、JSON API、STOP/E-STOP送信を統合した。
- 実機: COM4の通信側 XIAO ESP32-S3 Sense へビルド済みファームウェアを書込み成功。SoftAPは `XIAO-BOAT-TELEMETRY`、URLは `http://192.168.4.1/`。
- 証拠: 書込み後に作成された `D:\BOATLOG\RUN0008.BIN` は9,210,423 bytes、106,986レコードで末尾まで完全に解析できた。BNO加速度52,251、gyro26,912、quat9,426、GNSS生文14,378、GNSS fix3,579、GNSS状態440を含む。最終GNSS fix は有効フラグ `0x7FF`、HDOP 0.5、fix type 3。
- 解釈: RUN0008のローカル連番の空き1,552個は、SDへ記録しない通信側→制御側Heartbeatが使った連番であり、SD書込み落ちの証拠ではない。以後は送信方向とログ方向の連番を分離するよう改修済み。

このログには、実施した作業、判断、検証条件、結果、次の行動を追記する。新しい記録を先頭に追加する。未実施・未検証の内容を成功として書かない。

## 2026-07-20 — 全体縦切りを先行する方針への変更

- 判断: 詳細な周期最適化や段階試験を先行させず、GNSSを含む2台全体の形を早く作る。全体が動かなければ、個別の詳細最適化に意味がないためである。
- 根拠: 通信側Senseでは、GNSS=D0/D1、比較BNO=D2〜D5、制御側UART=D7/D6、SD=GPIO21/D8〜D10と割り当てられ、既存の実証済みコード間でピン競合がない。
- 実施方針: UART→SD成功基盤を保持し、新しい通信・記録側統合プロジェクトへGNSS、比較BNO、SoftAP/Web UIを同居させる。制御側には最小のSTOP/E-STOP受信を追加する。
- 次: `SYSTEM_VERTICAL_SLICE_PLAN.md` に従い、通信・記録側統合プロジェクトを作る。

## 2026-07-20 — 実機BOATLOGの解析

- 実施: `D:\BOATLOG\RUN0001.BIN`〜`RUN0007.BIN`を、通信側の保存形式に従って直接復号した。全RUNの構造完全性、Boot ID、制御側グローバルシーケンス、RUN0007の実測Hzと最大受信間隔を `BOATLOG_ANALYSIS.md` に記録した。
- 結果: RUN0007は121.377秒・23,660レコードで、制御側シーケンス`62652`〜`86311`が欠番なし。制御側→UART→通信側→SDの保存経路が、この区間で取りこぼしなく動作した直接証拠である。
- 留意: RUN0001〜0006には保存フレーム間のシーケンス欠番がある。原因はこのBIN単独では特定できず、RUN0007の成功を全RUNへ遡及しない。
- 次: 制御側統合では、RUN0007の完全性確認を試験終了時の必須判定にし、センサ実測Hz・最大間隔を改善・評価する。

## 2026-07-20 — 通信側UART→SD保存の証拠探索

- 実施: ワークスペース内の通信ロガー関連テキスト、BIN/CSV/TXT/LOG、ビルド成果物、接続中ストレージの保存フォルダを確認した。結果を `VALIDATION_EVIDENCE.md` に記録した。
- 確認できた証拠: 通信側`src/main.cpp`更新後17秒以内に`main.cpp.o`、`firmware.bin`、`firmware.elf`が生成されており、現ソースは2026-07-19にビルドされた。ソースにはUART受信専用タスク、バッファ、キュー、SDバッファ、エラーカウンタがある。
- 見つからなかった証拠: 成功した`BOATLOG/RUNxxxx.BIN`、対応CSV/TXT、シリアルモニタ出力、ドロップ0／SDエラー0を示す停止サマリー。接続中の`G:`はGoogle Driveであり、SDカードの`BOATLOG`は存在しなかった。
- 判断: 過去の実機成功はユーザー報告として有効な基盤情報だが、現Gitリビジョンでの独立した再現証拠は未取得。成功ログを回収または再現試験すると確実な証拠になる。

## 2026-07-20 — 通信・記録側UART→SD基盤の認識訂正

- 事実（ユーザー報告）: 通信側XIAOは、制御側XIAOからUARTで受け取ったテレメトリーをmicroSDへ保存する橋渡しとして開発した。SD保存の問題を切り分けた後、最終的に取りこぼしなく保存できる状態へ到達していた。
- 判断: この受信ロガーは、制御側統合試験で使用する既存の成功基盤であり、現時点の再実装対象ではない。GNSS、比較BNO、双方向操作、時刻同期などだけを後続拡張として扱う。
- 留意: 成功時のファームウェア識別子、試験条件、ログファイルと現Gitリビジョンの同一性は未照合。今後、制御側を接続する前に破壊的変更なしで再現確認する。

## 2026-07-20 — 現在の主作業の訂正

- 判断: 現在進めているのは、`xiao-boat-control-integration` を制御側XIAOとして作る統合の第一段階である。通信・記録側の完成や2台全体の統合を現在の主作業として扱わない。
- 反映: `PROJECT_CONTEXT.md` と `WORK_PLAN.md` の現在地、次の作業、実施順序を制御側優先へ訂正した。
- 次: 制御側XIAOに必要な統合差分を、既存の実装・実機成功済み単体コードを根拠に実装順へ分解する。

## 2026-07-20 — 統合コードの現状レビュー

- 実施: `xiao-boat-control-integration` と `xiao-boat-telemetry-sd-logger` のソース、設定、プロトコルを、確定済みの2台構成と統合試験要件へ照合した。結果を `INTEGRATION_GAP_REVIEW.md` に記録した。
- 判断: 現在は片方向の制御側→記録側テレメトリーと手動ベンチ試験の土台段階である。双方向リンク、通信断安全、時刻同期、統合計測指標、GNSS/比較BNO、解析ツールは、統合試験の完了済み機能として扱わない。
- 検証: PlatformIO CLIがこの環境のPATHにないためビルド未実施。実機接続・実機試験も未実施。
- 次: UART payloadとBINレコード仕様を合意し、制御側の受信／通信断安全を最初の実装範囲として確定する。

## 2026-07-20 — 引継ぎ台帳の導入

- 実施: `PROJECT_CONTEXT.md`、`WORK_PLAN.md`、`WORK_LOG.md`を新設し、READMEとAGENTS.mdに確認・更新手順を追加した。
- 判断: コンテキスト切替時の正本をこの3ファイルに固定する。コード・設定・実機検証・重要判断を変更した作業では、計画とログを同じ変更に含める。
- 検証: ファイルの作成とリンクの追加のみ。実機・ファームウェアのビルドは実施していない。
- 次: `WORK_PLAN.md` の順序1として、制御側統合コードと通信・記録側の役割・制約差分をレビューする。

## 2026-07-20 — Git管理とGitHub公開

- 実施: ルートGitリポジトリを初期化し、`.pio`、IDE状態、Pythonキャッシュ、外部参照リポジトリを除外して初回コミット `2c64914` を作成。公開リポジトリ `https://github.com/temesotejam/proglam` の `main` へpushした。
- 判断: `_reference_cores3_vl53l5cx_distance_map` と `_reference_vedderb_bldc` は独立した外部参照であり、このリポジトリには含めない。
- 検証: `main` は `origin/main` を追跡し、公開設定を確認済み。既存Markdownの末尾空白を空白チェックが指摘したが、内容を改変せずコミットした。
- 次: プロジェクトの最終目的・2台構成・統合試験順序を台帳へ反映する。

## 2026-07-23 固定条件実験ファームウェアを作成

- origin/agent/benchmark-stability の診断コードを基に、制御側・通信側で同名のPlatformIO環境を選ぶ16個の固定条件ファームウェアを追加した。
- 制御側の共有I2Cは起動時から環境の指定速度に固定し、BenchmarkPrepare は受信条件の照合だけを行う。測定中には Wire.end()、I2C速度変更、INA/ToF再初期化を実行しない。
- P0の制御側・通信側をビルドした。実機への書込み・測定は未実施。


## 2026-07-24 P0 通信側を書込み

- 通信側XIAOを COM5（USB Serial Device、VID:303A/PID:1001）として認識し、xiao-boat-telemetry-integration の p0_bringup_400k を書き込んだ。
- シリアル出力で SD=1、GNSS=1、BNO=1、制御側UARTフレーム受信を観測した。
- 制御側は未書込みのため、P0全体の成立、SoftAP/Web UI、STOP/E-STOP、BENCHログの合格判定は未検証。

- 制御側XIAOのUSB接続では、COMポートおよびESP32/XIAO系PnPデバイスが検出されなかった。データ通信対応ケーブル・USBポートを確認し、PC再起動後に再検出する。通信側P0（COM5）の書込み・起動確認は完了済みとして保持する。

## 2026-07-25 — ToF実測レート低下の診断環境を追加

- P3の有効反復では、ToF 4x4/30 Hz要求に対してToF・INAとも約8.3 Hzとなった。`getRangingData()` の最大時間は約13.4 msであり、4バイトを読む `isDataReady()` 自体にはライブラリ上の待機ループがないことを確認した。
- 制御側に、センサから読戻す解像度・周波数・I2C転送分割サイズと、ready判定およびToFサービス全体の最大時間を `BenchmarkEvent` として記録する診断を追加した。`BenchmarkReady` も期待値ではなく読戻し値を報告する。
- 両プロジェクトに、共有I2C 400 kHz・ToF 4x4/30 Hz・DRY_RUN・2分の `p3_diag_4x4_30`（`BOAT_EXPERIMENT=16`）を追加した。制御側・通信側ともビルド成功（RAM 33.2%/56.5%、Flash 16.0%/25.7%）。実機書込み・測定は未実施。

## 2026-07-25 — ToF診断用の通信側を書込み

- 通信側XIAO ESP32-S3 SenseをCOM5として認識し、`xiao-boat-telemetry-integration` の `p3_diag_4x4_30` を書き込んだ。859,264 bytesの書込みデータはハッシュ検証まで成功した。
- 通信側は共有I2C 400 kHz・ToF 4x4/30 Hz・DRY_RUN・2分の診断キャンペーン待機状態である。制御側を同名環境へ書き込むまで開始しない。

## 2026-07-25 — ToF診断用の制御側を書込み

- 制御側XIAO ESP32-S3をCOM4（MAC 34:85:18:AB:FA:90）として認識し、`xiao-boat-control-integration` の `p3_diag_4x4_30` を書き込んだ。535,712 bytesの書込みデータはハッシュ検証まで成功した。
- 制御側・通信側とも共有I2C 400 kHz・ToF 4x4/30 Hz・DRY_RUN・2分の同名診断環境となった。通信側Web UIで開始後、生成されるRUNのBIN/TXTから診断値を確認する。

## 2026-07-25 — RUN0015を解析、ToFを自律モードへ修正

- RUN0015は64,667レコードを末尾まで構造的に復号できた。`normal_stop=1`、SD書込みエラー0、キューdrop 0、I2Cエラー0、リンクdrop 0、ToF不完全フレーム0で完走している。
- 設定読戻しはToF 4x4・30 Hz、I2C転送分割サイズ32 bytesで一致した。しかし測定窓119.989秒のToF/INAはともに998フレーム、実測8.318 Hz、ToF最大周期133.535 msだった。
- 追加診断はready判定最大304 us、ToFサービス全体最大13,724 us、データ読出し最大13,439 usを示した。よって約120 ms周期の主因はI2C待機・制御ループの詰まりではない。
- 原因判断: 周波数を指定する測定でToFを`CONTINUOUS`モードにしていた。ULDの連続モードでは周期が設定周波数ではなく連続測距条件に従う。`AUTONOMOUS`モードへ修正し、4x4/30 Hzを周期制御するようにした。制御側 `p3_diag_4x4_30` のビルドは成功（RAM 33.2%、Flash 16.0%）。
- 修正版のCOM4書込みは、書込み開始時にCOM4が存在せず失敗した。実機書込み・再診断は未実施。

## 2026-07-25 — ToF自律モード修正版の制御側を書込み

- 制御側XIAO ESP32-S3をCOM4として再認識し、`p3_diag_4x4_30` の自律モード修正版を書き込んだ。535,712 bytesの書込みデータはハッシュ検証まで成功した。
- 通信側は記録ファームウェアを再書込みせず、そのまま使用する。両機をUART接続してDRY_RUNの2分診断を再実行し、4x4/30 Hzの実測周期を確認する。

## 2026-07-25 — RUN0016/RUN0017を解析、積分時間20 msの診断版を準備

- RUN0016はSDの`buffer_full`書込みで1回失敗し、結果フレームなし・末尾37 bytesの不完全レコードとなったため無効とした。キューdropは0である。
- RUN0017は62,085レコードを末尾まで復号でき、`normal_stop=1`、SD/キュー/I2C/リンクエラー0、ToF不完全0で完走した。4x4/30 Hzの設定読戻しは一致したが、ToF/INAとも1,004フレーム・8.372 Hz、最大ToF周期132.098 msで、候補基準を満たさなかった。
- 自律モードでは積分時間を精密に設定でき、ライブラリは積分時間が選択周波数の周期より短いことを要求する。従来の約120 ms周期は既定の約100 ms積分と整合するため、4x4/30 Hzでは積分時間を20 msに設定する。解像度・周波数・積分時間・モードを読戻し確認し、積分時間とモードを診断イベントcode 22に記録する。
- 制御側 `p3_diag_4x4_30` の20 ms積分時間版をビルドした（RAM 33.2%、Flash 16.0%）。実機書込み・測定は未実施。

## 2026-07-25 — ToF積分時間20 ms版の制御側を書込み

- 制御側XIAO ESP32-S3をCOM4として認識し、`p3_diag_4x4_30` の自律モード・積分時間20 ms版を書き込んだ。536,224 bytesの書込みデータはハッシュ検証まで成功した。
- 通信側の記録ファームウェアは変更不要である。両機をUART接続してDRY_RUNの2分診断を実行し、diagnostic event code 22の20 ms/自律モード読戻しと実測ToF周期を確認する。

## 2026-07-25 — RUN0018を解析、メインループ時間トレースを準備

- RUN0018は64,567レコードを末尾まで復号でき、`normal_stop=1`、SD/キュー/I2C/リンクエラー0、ToF不完全0で完走した。診断event code 22は積分時間20 ms、モード値1（SparkFunライブラリの自律モード）を確認した。
- それでもToF/INAとも993フレーム・8.275 Hz、最大ToF周期134.387 msで候補基準を満たさなかった。ready判定最大311 us、ToFサービス全体最大13,714 usであり、ToF設定・読出し処理は主因ではない。
- ToFとINAが同一周期で低下するため、共通メインループの停止を対象にした。BNO08Xは100 kHz I2Cで1周最大24イベントを処理するため有力候補であるが、変更前に実測で確認する。
- 制御側にBNOポーリング、UART受信、BNO復旧、VESC、メインループ全体の最大時間を記録するdiagnostic event code 23〜25を追加した。`p3_diag_4x4_30` のビルド成功（RAM 33.2%、Flash 16.0%）。実機書込み・測定は未実施。

## 2026-07-25 — メインループ時間トレース版を制御側へ書込み

- p3_diag_4x4_30 の制御側ファームウェアをCOM4（MAC `34:85:18:AB:FA:90`）へ書き込んだ。
- 536,416 bytes のアプリケーション書込み後、esptoolのハッシュ照合が成功した。
- 通信側は既存の診断受信ファームウェアのままとし、次回の2分DRY_RUNでイベントcode 23〜25を回収する。

## 2026-07-25 — RUN0019を解析、BNO08Xポーリングを上限化

- RUN0019は65,955レコードを末尾まで構造的に復号でき、`normal_stop=1`、SD書込みエラー0、キューdrop 0、失敗フェーズ0で2分診断を完走した。
- ToF設定は4x4・30 Hz・自律モード・積分20 msの読戻しと一致したが、ToFは994フレーム（8.289 Hz、最大周期214.414 ms）、INAは995フレーム（8.298 Hz、最大周期130.264 ms）で候補基準を満たさない。
- 診断event code 23はUART受信最大6,069 us、BNOポーリング最大109,027 usを記録した。event code 24はBNO復旧最大13 us、VESC最大2,115 us、code 25はメインループ最大128,842 usである。従って共通の約120 ms停止はBNO08Xの一周最大24イベント一括処理が原因であり、ToF・INA・UART・VESCは主因ではない。
- BNO報告設定は維持したまま、`kBnoPollEventBudget=2`を追加して1周の処理を最大2イベントへ制限した。ToFを飢餓状態にせず、次RUNでBNO連番欠落を確認する最小変更である。
- 制御側 `p3_diag_4x4_30` の修正版をビルドした（RAM 33.2%、Flash 16.0%）。実機書込み・再診断は未実施。

## 2026-07-25 — BNO08X上限版を制御側へ書込み

- `p3_diag_4x4_30` のBNO08Xポーリング上限版をCOM4（MAC `34:85:18:AB:FA:90`）へ書き込んだ。
- 536,448 bytes のアプリケーション書込み後、esptoolのハッシュ照合が成功した。
- 通信側は既存の診断受信ファームウェアを継続使用し、次の2分DRY_RUNでToF/INA周期とBNO連番を再評価する。

## 2026-07-25 — RUN0020を解析、ToF優先・BNO 50 Hz版を準備

- RUN0020は54,455レコードを末尾まで構造的に復号でき、`normal_stop=1`、SD書込みエラー0、キューdrop 0、失敗フェーズ0で完走した。
- BNO上限2イベント化により、BNOポーリング最大は109.027 msから23.525 ms、メインループ最大は128.842 msから43.254 msへ低下した。ToFは2,607フレーム・21.717 Hz（最大周期66.847 ms）、INAは3,380フレーム・28.156 Hz（最大周期44.390 ms）へ改善した。
- ToFの30 Hz候補基準（27 Hz以上）には未達である。BNO処理がToFより前にあり、最大23.525 msのBNO処理がToFの33.3 ms周期を遅延させるため、ToF/INAを先行させる。
- BNO08Xは加速度・ジャイロ200 Hz、回転50 Hzの計450 reports/sを要求していたが、100 kHz I2Cの実測処理能力に対して過大である。3種類とも50 Hzへ下げ、1周の取得は1イベントに制限する。これにより合計150 reports/sとし、ToF 30 Hzを優先する。
- 制御側 `p3_diag_4x4_30` の修正版をビルドした（RAM 33.2%、Flash 16.0%）。実機書込み・再診断は未実施。

## 2026-07-25 — ToF優先・BNO 50 Hz版を制御側へ書込み

- `p3_diag_4x4_30` のToF優先・BNO08X各報告50 Hz・1周1イベント版をCOM4（MAC `34:85:18:AB:FA:90`）へ書き込んだ。
- 536,416 bytes のアプリケーション書込み後、esptoolのハッシュ照合が成功した。
- 通信側は既存の診断受信ファームウェアを継続使用し、次の2分DRY_RUNでToF 30 Hz候補基準を再評価する。

## 2026-07-25 — RUN0021でToF 30 Hz候補基準を達成

- RUN0021は60,271レコードを末尾まで構造的に復号でき、`normal_stop=1`、SD書込みエラー0、キューdrop 0、失敗フェーズ0で完走した。
- ToFは3,602フレーム・30.008 Hz、平均周期33.324 ms、最大周期39.070 ms。INAも3,602フレーム・30.008 Hz、最大周期39.058 msだった。ToFの30 Hz候補基準（27 Hz以上、最大周期66.7 ms未満）を満たす。
- 診断はBNOポーリング最大7.754 ms、UART受信最大4.135 ms、BNO復旧最大39 us、VESC最大2.024 ms、メインループ最大24.316 msを記録した。前RUNより全てToF周期を阻害しない範囲である。
- BNOのログ受信率は加速度118.001 Hz、ジャイロ51.478 Hz、回転32.016 Hzだった。設定値は各50 Hz・1周1イベントであり、BNOの実際のレポート発行挙動は別途データ解析対象とするが、ToF/INA 30 Hz達成と通信・SD健全性は両立した。
- 同一条件をさらに2回反復して、ToF/INA 30 Hz候補基準の再現性を確認する。

## 2026-07-25 — RUN0022〜RUN0024の反復確認

- RUN0022は55,843レコードを完全復号し、`normal_stop=1`、SD書込みエラー0、キューdrop 0で完走した。ToF/INAはともに30.016 Hz、最大周期は40.081/40.069 ms、BNOポーリング最大7.656 ms、メインループ最大23.632 msだった。
- RUN0023はSD `buffer_full` による書込み失敗（`sd_write_errors=1`）で中断し、結果フレームを持たないため無効とした。BINは中断時点の41,388レコードまで構造的に復号できたが、試験必須条件のSDエラー0を満たさない。
- RUN0024は55,755レコードを完全復号し、`normal_stop=1`、SD書込みエラー0、キューdrop 0で完走した。ToF/INAはともに30.007 Hz、最大周期は40.964/40.973 ms、BNOポーリング最大7.645 ms、メインループ最大22.780 msだった。
- RUN0021・RUN0022・RUN0024の3有効RUNでToF/INA 30 Hz候補基準を再現した。一方、RUN0023のSD書込み失敗を置換するため、同一条件でもう1回2分の確認RUNを行う。

## 2026-07-25 — RUN0025でSD失敗の置換確認を完了

- RUN0025は58,171レコードを末尾まで構造的に復号でき、`normal_stop=1`、SD書込みエラー0、キューdrop 0、ログfaultなし、失敗フェーズ0で完走した。
- ToF/INAはともに3,603フレーム・30.013 Hz、ToF最大周期40.132 ms、INA最大周期40.143 msだった。BNOポーリング最大7.646 ms、メインループ最大23.355 msで、30 Hz周期を阻害していない。
- RUN0021、RUN0022、RUN0024、RUN0025の4有効なQUICK RUNすべてで4x4/30 Hz候補基準とSD/UART/キューエラー0を達成した。RUN0023はSD `buffer_full`の中断として引き続き無効記録とする。
- よって、現在のハードウェアと約10 cm直接配線では、ToF 4x4・30 Hz、自律モード・積分20 ms、ToF優先、BNO 1周1イベントの構成をP3後段候補として採用できる。正式計画の5分反復は未実施である。

## 2026-07-25 — INAは現行設定を維持し、VESCテレメトリを優先

- 現行INAは約30 Hz、I2Cエラー0であり、ユーザー要求の低速で確実な電流監視には十分と判断した。P4の高更新レート比較は、必要になった時点まで後回しにする。
- 次の優先はVESCの受動テレメトリ取得とする。DRY_RUN・Duty 0で、入力電圧、入力/モータ電流、eRPM、温度、故障コード、UART応答時間・エラーを記録する。
- VESCの電圧・電流は消費電力・状態推定に用いる。水上速度そのものは直接は得られないため、eRPMを駆動系の既知比とGNSS速度で較正し、GNSSと融合して推定する。

## 2026-07-25 — VESC受動通信の初回測定時間を3分に決定

- モータ未接続の初回試験では、熱・推進負荷の長時間変動は評価対象外である。
- VESC状態要求は50 Hz設定のため、3分で約9,000回の要求機会を得られる。通信の応答率、CRC/タイムアウト、電圧値の安定性を確認するには十分であり、初回は5分ではなく3分とする。
- 5分以上は、モータ接続後の温度・電圧降下・負荷変動を含む試験で用いる。

## 2026-07-25 — VESC受動3分測定プログラムを追加

- 両プロジェクトへ `vesc_passive_3min`（BOAT_EXPERIMENT=17、3分、400 kHz、ToF 4x4/30 Hz維持）を追加した。
- 制御側は開始時のVESC要求・応答・エラー数を保存し、終了時に診断event code 26（要求数/応答数）およびcode 27（エラー数）を記録する。VESC状態フレームには電圧、電流、eRPM、温度、Duty、故障コードが保存される。
- DutyはDRY_RUN保護により0のまま。制御側・通信側の `vesc_passive_3min` ビルドに成功した。実機書込みは未実施。

## 2026-07-25 — BNO08X専用タスクと姿勢100 Hz測定環境を実装

- 制御側のBNO08X `poll()` と `recover()` をメインループから専用FreeRTOSタスクへ移した。BNO専用I2C（D4/D5、100 kHz）だけをこのタスクが所有するため、共有I2C上のToF/INA/PCA処理とは競合しない。タスクはcore 0、優先度1、1 ms周期とし、UART送信タスク（優先度2）を優先する。
- 測定中はBNOタスク内の最大poll時間・最大recover時間を診断event code 23/24に記録する。メインループの最大時間はBNOの処理時間を含まない値となるため、二つを分けて評価できる。
- 両プロジェクトに同名の `bno_attitude100_gyro50_3min` 環境（BOAT_EXPERIMENT=20）を追加した。Game Rotation Vectorは100 Hz、較正ジャイロは50 Hzで要求する。出力は自前姿勢推定を使わない場合の「ジャイロ＋BNO姿勢」の組合せであり、ToF 4x4/30 Hz・INA低速確実取得・VESC受動取得を維持する。
- ビルド検証: 制御側はRAM 108,844 bytes（33.2%）、Flash 536,605 bytes（16.1%）で成功。通信・記録側はRAM 185,208 bytes（56.5%）、Flash 858,913 bytes（25.7%）で成功した。実機書込み・測定は未実施。
## 2026-07-25 — BNO専用タスク・姿勢100 Hz版を制御側へ書込み

- 制御側XIAO ESP32-S3をCOM4（MAC `34:85:18:AB:FA:90`）として確認し、`bno_attitude100_gyro50_3min` を書き込んだ。
- 536,976 bytesのアプリケーションを含む書込みは、全領域のハッシュ照合まで成功した。
- 通信・記録側は未書込みのため、3分DRY_RUNはまだ開始しない。通信側を同名環境へ揃えた後に開始する。
## 2026-07-25 — BNO専用タスク・姿勢100 Hz版を通信側へ書込み

- 通信・記録側XIAO ESP32-S3 SenseをCOM5（MAC `E0:72:A1:FC:08:D0`）として確認し、制御側と同じ `bno_attitude100_gyro50_3min` を書き込んだ。
- 859,280 bytesのアプリケーションを含む書込みは、全領域のハッシュ照合まで成功した。
- 両XIAOは同名・同条件の3分DRY_RUN測定待機状態である。UART接続を維持し、通信側Web UIで開始する。
## 2026-07-25 — RUN0004を解析、BNO専用タスク・姿勢100 Hz条件は合格

- `E:\BENCH\RUN0004.BIN/TXT` は180.072秒で正常完走した。BINは91,138レコードを末尾まで復号でき、`normal_stop=1`、SD書込みエラー0、キューdrop 0、ログfaultなし、制御側link drop 0、I2Cエラー0だった。
- 制御側がフレーム生成時に付与した時刻で評価した。BNO較正ジャイロは9,005件・50.010 Hz（最大周期24.791 ms）、Game Rotation Vectorは18,011件・100.021 Hz（最大18.358 ms）だった。両ストリームのBNOセンサ連番は全区間で連続し、欠番は0だった。
- ToFは5,403件・30.006 Hz（最大36.222 ms）、INAは5,403件・30.006 Hz（最大36.216 ms）、VESCは5,404件・30.007 Hz（最大37.322 ms）で、姿勢100 Hzとの同時動作でも従来の30 Hz要件を維持した。VESC要求/応答は5,403/5,404、エラー0だった。
- BNO専用タスクの最大poll時間は6.432 ms、最大recover時間は1.750 ms。メインループ最大時間は18.152 ms、ToFサービス最大13.826 ms、VESCサービス最大2.143 msだった。SDの最大1書込みは81.123 msで、通信側の受信時刻だけでは最大約208 msの見かけの間隔が出るが、制御側生成時刻・BNO連番・リンクdrop 0によりデータ欠損ではないと確認した。
- 判定: 姿勢100 Hz＋ジャイロ50 Hz、ToF/INA/VESC各30 Hzの構成は、この3分DRY_RUNで合格。次の比較はジャイロも100 Hzへ上げた条件とする。
## 2026-07-25 — BNO姿勢・ジャイロ100 Hz比較版を追加

- 両プロジェクトに `bno_attitude100_gyro100_3min`（BOAT_EXPERIMENT=21、3分、ToF 4x4/30 Hz、INA/VESC条件はRUN0004と同一）を追加した。
- 制御側だけを変更し、BNO08Xの較正ジャイロとGame Rotation Vectorをともに10 ms間隔（100 Hz）で要求する。BNO専用タスク、1イベント上限、ToF/INA優先、DRY_RUN保護は維持する。
- 制御側・通信側ともビルドを完了した。実機書込み・測定は未実施。書込み時点でCOM4/COM5がWindowsに認識されていなかったため、再接続後に両機へ同名環境を書き込む。
## 2026-07-25 — BNO姿勢・ジャイロ100 Hz版を制御側へ書込み

- 制御側XIAO ESP32-S3をCOM4（MAC `34:85:18:AB:FA:90`）として確認し、`bno_attitude100_gyro100_3min` を書き込んだ。
- 536,976 bytesのアプリケーションを含む書込みは、全領域のハッシュ照合まで成功した。
- 通信・記録側は未書込みのため、3分DRY_RUNはまだ開始しない。通信側を同名環境へ揃えた後に開始する。
## 2026-07-25 — BNO姿勢・ジャイロ100 Hz版を通信側へ書込み

- 通信・記録側XIAO ESP32-S3 SenseをCOM5（MAC `E0:72:A1:FC:08:D0`）として確認し、制御側と同じ `bno_attitude100_gyro100_3min` を書き込んだ。
- 859,280 bytesのアプリケーションを含む書込みは、全領域のハッシュ照合まで成功した。
- 両XIAOは同名・同条件の3分DRY_RUN測定待機状態である。UART接続を維持し、通信側Web UIで開始する。
## 2026-07-25 — RUN0005/RUN0006を解析、ジャイロ100 Hz条件は保留

- RUN0005はSD `buffer_full` による`SD write failed`で中断した。39,190レコードまでは構造復号できたが、末尾に68 bytesの不完全レコードがあり、BenchmarkResultがない。SD書込みエラー1のため無効とする。
- RUN0006は104,176レコードを末尾まで完全復号し、180.059秒で正常完走した。SD書込みエラー0、キューdrop 0、制御側link drop 0、I2Cエラー0、VESCエラー0だった。
- 制御側生成時刻で、BNOジャイロは18,007件・100.002 Hz（最大16.937 ms）、姿勢は18,009件・100.018 Hz（最大15.214 ms）だった。姿勢のBNOセンサ連番は連続した一方、ジャイロにはsequence step=2が3回あり、3サンプルの欠番を確認した。
- 周辺系はToF 5,402件・30.004 Hz（最大36.208 ms）、INA 5,402件・30.004 Hz（最大36.242 ms）、VESC 5,403件・30.004 Hz（最大37.184 ms）で維持した。BNOタスク最大poll/recoverは6.206/2.333 ms、メインループ最大19.162 ms、ToFサービス最大13.818 ms、VESC最大2.151 msだった。
- 判定: 100 Hz＋100 Hzは周辺センサを落とさず動作するが、初回有効RUNでジャイロ3欠番があるため「欠番0」の安定条件には未達。RUN0005のSD失敗は別途無効記録とし、次は同条件の再反復で欠番再現性を確認する。必要ならBNOタスク優先度またはINT駆動へ進む。
## 2026-07-25 — RUN0007で100 Hzジャイロ欠番を再確認

- RUN0007は106,617レコードを末尾まで完全復号し、180.070秒で正常完走した。SD書込みエラー0、キューdrop 0、制御側link drop 0、I2Cエラー0、VESCエラー0だった。
- ジャイロは18,009件・100.007 Hz（最大17.575 ms）で、BNOセンサ連番step=2が2回、合計2サンプルの欠番を確認した。姿勢は18,010件・100.019 Hz（最大14.624 ms）で連番欠番0だった。
- ToF/INA/VESCは各5,403件・30.006 Hz、最大38.276/38.257/38.065 msで安定した。BNO最大poll/recoverは6.714/2.335 ms、メインループ最大19.203 ms、ToFサービス最大13.839 ms、VESC最大2.162 msだった。
- RUN0006/RUN0007の有効2反復で、100 Hzジャイロの欠番は3回/2回（計5回/36,014区間）と再現した。姿勢100 Hzおよび周辺30 Hzは欠損なし。
- 判断: 現状の1 ms周期・優先度1のBNOタスクでは、100 Hzジャイロを「欠番0」とは保証できない。次の最小変更はBNOタスク優先度をUART送信タスクより上げること。その後も再現する場合はD3のBNO INTで起床する通知駆動へ移行する。実装は未着手。
## 2026-07-25 — 過去BNOプログラムのINT利用を確認

- `2026_05_20_xiao_esp32s3_bno08x_gpio4_gpio5_test-master` はD3へ`attachInterrupt(..., CHANGE)`を設定し、INTの立下り/立上りを記録していた。ただしISRはエッジ数を数えるだけで、`poll()`はメインループから常時呼ばれる。INTでI2C読出しまたは取得タスクの起床はしていない。
- `xiao_esp32s3_bno08x_bringup` はINTを入力にし、ライブラリに`setInterruptPin()`が存在する場合だけ設定を渡すが、ソース自身に「events are polled in loop」と明記されている。これもINT通知駆動の取得ではない。
- よって、このワークスペースにはINT配線の確認例はあるが、INTでBNO取得タスクを起床して欠番0を実証した過去プログラムはない。
- 方針: 制御側BNOを最高優先度の単独所有タスクとし、D3 ISRはI2Cを実行せずタスク通知だけを発行する。タスクが通知または短いタイムアウトで起床してBNO I2Cを読出す。自前推定用は加速度＋ジャイロ100 Hz、BNO内蔵推定用はジャイロ＋姿勢100 Hzを別条件で評価し、三出力同時100 Hzは採用しない。
## 2026-07-25 — BNO D3 INT通知駆動・加速度＋ジャイロ100 Hzを実装

- BOAT_EXPERIMENT=22（`bno_accel100_gyro100_int_3min`、180秒）を両プロジェクトに追加した。制御側BNOは較正ジャイロと加速度を各100 Hzで要求し、姿勢出力は要求しない。ToF 4x4/30 Hz、INAの低速・確実な監視、VESCのDuty 0受動取得は維持する。
- 制御側ではBNO専用タスクをcore 0・優先度3（UART送信タスクは優先度2）とした。D3のFALLING ISRはタスク通知だけを発行し、I2Cアクセスは一切しない。タスクはINT通知または2 msのフォールバック時間で起床し、INTがLowの間は継続してBNOを処理する。
- 測定結果には診断event code 28としてINTエッジ数とフォールバック起床回数を保存する。従来のcode 23/24のBNO poll/recover最大時間も継続する。
- ビルド検証: 制御側はRAM 109,452 bytes（33.4%）、Flash 538,513 bytes（16.1%）で成功。通信・記録側はRAM 185,208 bytes（56.5%）、Flash 858,913 bytes（25.7%）で成功。
- 書込み: 制御側XIAOをCOM4（MAC `34:85:18:AB:FA:90`）として確認し、全領域のハッシュ照合まで成功した。通信・記録側COM5はこの時点でWindowsに認識されず、未書込み。実機の3分測定は未実施であり、欠番改善の判定はまだしない。
## 2026-07-25 — BOAT22を通信・記録側へ書込み

- 通信・記録側XIAO ESP32-S3 SenseをCOM5（MAC `E0:72:A1:FC:08:D0`）として確認し、`bno_accel100_gyro100_int_3min` を書き込んだ。
- 859,280 bytesのアプリケーションを含む書込みは全領域のハッシュ照合まで成功した。制御側COM4も同一環境を書込み済みである。
- 両XIAOは3分DRY_RUN測定の開始待機状態である。測定結果を回収するまで、INT駆動による100 Hzジャイロ・加速度の欠番改善は未判定とする。
## 2026-07-25 — RUN0008でBNO INT通知駆動を確認

- `E:\BENCH\RUN0008.BIN/TXT` は180.033秒で正常完走した。BINは111,243レコードを末尾まで完全復号でき、TXTのrecordsと一致する。SD書込みエラー0、キュードロップ0、ログfaultなし、制御側link drop 0、I2Cエラー0だった。
- 制御側生成フレームを測定窓で評価した。較正ジャイロは18,008件・100.023 Hz（最大16.591 ms）でBNOセンサ連番の欠番0。以前のRUN0006/RUN0007で計5件あった100 Hzジャイロ欠番は、本RUNでは再現しなかった。
- 加速度は22,773件・126.489 Hz（最大20.097 ms）、BNOセンサ連番の欠番0だった。要求は100 HzだがBNO実出力は約126.5 Hzである。自前推定ではこの実測周期を用いるか、消費側で明示的に100 Hzへ間引く必要がある。
- ToF/INA/VESCの制御側出力はそれぞれ5,402/5,402/5,401件・30.007 Hz、最大35.169/35.218/36.244 ms。ToF不完全0、VESC要求/応答5,401/5,401、VESCエラー0だった。INAは5,401読出し中の新規変換が1,209、重複4,192であり、低速・確実な監視として維持する。
- BNO最大poll/recoverは6.659/0.014 ms、メインループ最大16.950 ms、ToFサービス最大13.803 ms、VESCサービス最大2.092 ms。INTエッジ81,551回、2 msフォールバック起床9,792回を記録した。INTはBNOデータイベントより多く発生するが、欠番・周辺系エラーは発生していない。
- 判定: D3 INT通知＋優先度3のBNO専用タスクは、3分DRY_RUNにおいてジャイロ100 Hzを欠番0で取得できた。次は同じ駆動方式でジャイロ＋姿勢各100 Hzを比較する。
## 再起動後の再開点

- 両XIAOにはBOAT22 `bno_accel100_gyro100_int_3min` が書込み済み。制御側はCOM4/MAC `34:85:18:AB:FA:90`、通信・記録側はCOM5/MAC `E0:72:A1:FC:08:D0`。
- 最新の有効結果は `E:\BENCH\RUN0008.BIN/TXT`。INT通知駆動の加速度＋ジャイロ測定は完了・解析済みで、ジャイロ100.023 Hz・欠番0を確認した。
- 次の実装・測定は、同じD3 INT通知駆動で「ジャイロ＋Game Rotation Vector（姿勢）を各100 Hz、3分」の比較条件を追加し、両基板へ書込み後にRUNを解析すること。加速度＋ジャイロ条件を再実行する必要はない。
## 2026-07-26 -- RUN0014 BOAT24 timing diagnostic (FAILED: control link timeout)

- Analysed user-provided `E:\BENCH\RUN0014.BIN/TXT` end to end. The 805,910-byte BIN parses exactly to 8,403 frames with no trailing or malformed record; therefore the conclusions below are based on the complete captured log.
- This was an aborted measurement, not a 60-s BOAT24 pass: `normal_stop=0`, `benchmark_outcome=link_timeout`, one phase, recorder-time span 20.236614 s. SD errors=0, logger queue drops=0, BNO decode errors=0, and BNO event queue drops=0.
- Communication-local BNO remained healthy during the observed interval: accel 2,517 (124.501 Hz), gyro 2,022 (99.991 Hz), magnetic 506 (24.994 Hz), all with observed sequence loss 0. It nevertheless has a 103.418-ms magnetic logged-time gap, so the 80-ms continuity gate is not passed.
- Control-origin data prove a transmit scheduling backlog: only accel/gyro/magnetic 776/614/154 frames arrive. Their control-side frame creation times span 6.13 s, while their recorder arrival times span 20.18 s. Only 62 control heartbeats arrive, versus 147 communication heartbeats. The bidirectional physical receiver is not generally failing because control frames continue to decode; instead, the control transmit FIFO is not drained in real time and its heartbeat is delayed until the communication-side link watchdog aborts.
- Code inspection identifies the scheduling inversion: control `bnoTask` priority 3/core 0 calls `linkSend()` directly; the sole FIFO drain `linkTxTask` is priority 2/core 0 and BNO's active-INT path has no delay. This can starve FIFO draining. No firmware change has been applied yet.
- New Type-21 timing diagnostics are zero because the largest queue wait was 76.313 ms, below their 80-ms emission threshold. The local magnetic gap still includes 70.510 ms from BNO frame generation to recording. Next diagnostic revision must use a 60-ms trigger while retaining the acceptance requirement of <=80 ms.
## 2026-07-26 -- RUN0014 mitigation build and control-node upload

- Implemented control UART scheduling fairness. `linkTxTask` is priority 4/core 0; BNO remains priority 3/core 0; BNO now yields one FreeRTOS tick after an active service. This directly addresses the RUN0014 condition where the lower-priority FIFO drain could not run while INT-driven BNO work was active.
- Changed communication `TimingDiagnostic` threshold from 80,000 us to 60,000 us, without changing BOAT24's <=80-ms pass criterion.
- BOAT24 builds succeeded: control 109,532 RAM / 539,749 flash bytes; communication 190,028 RAM / 863,557 flash bytes.
- Uploaded control firmware `0.3.3-control-link-fairness` to COM4, MAC `34:85:18:AB:FA:90`; all esptool hash checks passed. Serial diagnostics after reset: BNO=1, ToF frames increasing, INA errors=0, NAV TX/RX gap=0. `FAULT` while bench=0 is the expected failsafe state after an idle heartbeat timeout; the benchmark STOP/preflight sequence returns the control state to DISARMED.
- Communication firmware is built and awaits a connected COM5 for upload; no new measurement has been run yet.
## 2026-07-26 -- Communication upload for RUN0014 mitigation retry

- Uploaded communication firmware `0.3.3-bno-timing-60ms` to COM5, MAC `E0:72:A1:FC:08:D0`; image hash verification passed.
- Boot diagnostics showed `SD=1`, `GNSS=1`, `BNO=1`, log drop=0, and increasing control-UART receive count. Both boards are connected and ready; no benchmark was started automatically.
## 2026-07-26 -- RUN0015/RUN0016 analysis (FAILED before BOAT24 measurement)

- RUN0015: 17,061-byte BIN, 172 complete frames, recorder span 494.587 ms; `stop_ack_timeout`, `command_ack_rx=0`, normal_stop=0. RUN0016: 13,108-byte BIN, 150 complete frames, recorder span 509.802 ms; same failure.
- Both files contain the local Stop request (Type 36) plus local heartbeats; neither contains control CommandAck (Type 17) or control heartbeat. Only 3 / 6 ordinary control-origin frames arrive. Therefore neither run exercises the 60-s BNO, UART, SD, or Type-21 diagnostic acceptance path.
- Attempted live control COM4 diagnostic after analysis; COM4 no longer exists and Windows listed no serial ports. The next action is connection/power verification, not a firmware conclusion from these two aborted files.
## 2026-07-26 -- Control UART live verification after reconnect

- COM4 reappeared. Post-reset diagnostics for control firmware `0.3.3-control-link-fairness`: BNO=1, ToF incrementing, INA errors=0.
- Observed `nav` and `gnss_result_tx` increasing together at 10 Hz with nav error/gap=0, proving the communication-to-control UART direction is currently operating. No new benchmark was started automatically.
## 2026-07-26 -- RUN0017 analysis and control start-path FIFO fix

- RUN0017: 17,373 bytes, 175 complete BIN frames, 495.733-ms span, normal_stop=0, stop_ack_timeout, command_ack_rx=0. Local Stop and heartbeats are logged; no control ACK/heartbeat, only three control BNO frames.
- Direct control observation after the failure showed DISARMED, confirming STOP reception. Root cause is reply starvation behind idle control BNO frames in the shared UART FIFO.
- Uploaded control `0.3.4-control-bench-stream-gate` to COM4 / MAC `34:85:18:AB:FA:90`, hash verified. BNO UART emission is now benchmark-phase gated. Added `link=used/drops/high-water` to serial diagnostics.
- Post-upload live diagnostics: BNO=1, ToF increasing, INA errors=0, NAV/response advancing at 10 Hz with gaps=0; idle FIFO `link=0/0/2`. No new benchmark was started automatically.
## 2026-07-26 -- RUN0020を正式な安定基準に確定

- ユーザー確認により、RUN0020を内部センサ・基板間通信の正式な安定動作／比較基準として確定した。現行構成では両側BNO08Xの3ストリーム欠落0、UART NAV送信802回／結果受信802回・結果CRCエラー0、GNSS測位維持、ToF 1,800フレーム・I2Cエラー0、SD書込みエラー／キュードロップ0、制御側PASS、BOAT24通過である。
- PCからの一時的なUSB／SoftAP未接続はRUN0020の試験結果を否定しない。再接続後は構成を変えず、短時間の再現確認を行う。BOAT23は開始しない。

## 2026-07-26 -- BOAT24 RUN0020: 記録間隔ゲート合格

- RUN0018/0019のTimingDiagnosticから、SDの単発512-byte書込みが最大80 ms超まで伸び、SDタスクで記録時刻を採番すると実データの受信間隔まで長く見えることを確認した。通信側ログをcore 1 priority 2の専用タスクへ分離し、測定中の `File::flush()` を停止時のみに移した。
- さらにBINの外側タイムスタンプを、フレームが共通ログキューへ入った受信時刻 (`logQueueUs`) に変更した。SDタスク開始時刻 (`sdTaskUs`) とキュー待ちはフレーム／TimingDiagnosticに保持するため、SD待ちを隠さずに受信記録の時刻を正しく保存する。
- 通信側 `0.3.5-ingress-timestamp` をCOM4（MAC `E0:72:A1:FC:08:D0`）へハッシュ検証付きで書込み、SoftAP経由で `RUN0020.BIN/TXT` を取得した。測定フェーズは60.038519 s、`normal_stop=1`、`benchmark_outcome=completed`、制御側BenchmarkResultはPASSである。
- 48,727レコードを完全に解析した。通信側／制御側の加速度、ジャイロ、地磁気の全ストリームでsequence欠落0。地磁気の最大記録間隔は通信側46.989 ms、制御側UART由来53.575 msであり、80 ms以下を通過した。SD書込みエラー、ログキュードロップ、結果CRCエラー、BNOデコードエラー、BNOイベントキュードロップはすべて0である。
- SD書込み最大79.726 ms、最大ログキュー待ち123.600 ms、ログキュー高水位80/160、TimingDiagnostic 197件は残るが、キューで吸収されデータ欠落はない。BOAT24ゲートを合格とし、BOAT23は明示指示があるまで開始しない。

## 2026-07-26 -- BOAT24 RUN0018: 記録間隔ゲート不合格

- 通信側SoftAP APIで標準条件（`CABLE_10CM`、direct、DRY_RUN）を開始し、`/BENCH/RUN0018.BIN/TXT` を取得した。測定フェーズは60.055909 s、`normal_stop=1`、`benchmark_outcome=completed`、制御側BenchmarkResultはPASSである。
- TXT: records=48,631、SD書込みエラー=0、ログキュードロップ=0、結果CRCエラー=0、BNOデコードエラー=0、BNOイベントキュードロップ=0。BINは4,731,020 bytesを完全に48,631レコードとして解析できた。
- 測定フェーズのBNO sequence欠落は通信側／制御側とも加速度・ジャイロ・地磁気の全ストリームで0。地磁気は通信側1,500件/24.969 Hz、制御側1,501件/24.996 Hzである。一方、最大記録間隔は通信側158.076 ms、制御側164.476 ms、最大ログキュー待ちは124.516 ms、TimingDiagnosticは96件であり、BOAT24の80 ms上限を満たさない。
- 最大SD書込み時間は16.849 msで、124.516 msのログキュー待ちを単独では説明しない。記録側のログキュー／SDタスク遅延を次に計測・低減する。BOAT23へは進まない。

## 2026-07-26 -- 水上ボート姿勢状態推定の初期DRY_RUN実装

- ユーザー提供の設計書 `2026_07_26_水上ボート姿勢状態推定設計.md` に基づき、RUN0020を通信・センサ取得の安定基準として、制御側BNO08Xを主入力にする初期状態推定を実装した。BNOのコールバックから加速度・ジャイロ・地磁気を既存取得経路のまま推定器へ渡し、ジャイロ時刻を基準に四元数を更新する。加速度は重力方向補正に用い、地磁気は取付変換が未較正の間はヨー補正に用いない。GNSSとToFは推定状態へ鮮度付きで保持するが、この段階では姿勢を強制補正しない。
- プロトコルに `EstimatedState`（Type 22）を追加し、制御側は100 ms周期で姿勢・角速度・バイアス・GNSS/ToF補助値・各入力年齢・健全性を通信側へ送る。通信側の `/state` と `/api/estimated-state` はキャッシュ値のみを表示し、HTTPハンドラ内でI2C読出しを行わない。
- BNOの機体軸変換は物理取付をまだ実測していないため、恒等変換を仮置きにしたうえで `kBnoMountValidated=false` を固定した。従って初期状態の総合健全性は `DEGRADED` であり、姿勢値をアクチュエータ制御に使用しない。サーボ、VESC、その他の出力経路は変更していない。
- 検証: 制御側・通信側のPlatformIOビルドはともに成功した。今回の最終変更は実機へ未書込みであり、USB COMポートとSoftAPが未接続のため、ライブ値・UART・SD・取付軸較正の実機検証は未実施である。
- 次: 再接続後は構成を変えずに両方を書込み、短時間DRY_RUNで推定フレーム受信、`/state`、SD記録、既存RUN0020相当の健全性を確認する。その後、静止6姿勢と既知ヨー回転で取付変換を決めてから、`kBnoMountValidated` を有効化する。

## 2026-07-26 -- 制御側状態推定DRY_RUN版を書込み

- Windowsで唯一検出されたCOM5のESP32 MACを `esptool read_mac` で読み、`34:85:18:AB:FA:90`（制御側）と照合した。通信側と誤認して書込まないため、通信側がUSBで接続されるまではその書込みを保留する。
- 制御側へ `seeed_xiao_esp32s3` 環境を明示した `0.3.5-estimated-state-dry-run` を書込んだ。ブートローダ、パーティション、アプリケーションの各領域でesptoolハッシュ照合が成功した。書込み中にPlatformIOの環境指定なし実行が次のP0環境へ進み始めたため直ちに停止し、目的環境を明示した書込みで制御側を再度復旧した。他の試験環境への継続書込みは停止済みである。最終確認としてアプリケーション領域547,504 bytesを目的バイナリと `verify_flash` で読み取り照合し、digest一致を確認した。
- 起動後4秒の制御側診断は `dry=1`、BNO=1、ToF=39→58、INAエラー0、NAV/結果=80/80→121/121、NAV CRC/連番ギャップ=0/0、送信キューdrop=0であった。待機中の `FAULT` は通信側Heartbeat未受信時の既存安全フェイルセーフで、アクチュエータ出力はDRY_RUNのままである。
- 新しい通信側状態ページは未書込みの通信側ファームウェアを必要とするため、`/state` と `EstimatedState` のエンドツーエンド確認、SD記録、取付軸較正はまだ未実施である。

## 2026-07-26 -- 通信側書込みとEstimatedStateライブ確認

- 通信側をCOM4、MAC `E0:72:A1:FC:08:D0` と照合し、`seeed_xiao_esp32s3` 環境の状態受信・Web UI・SD記録版を書込んだ。ブートローダ、パーティション、870,384-byteアプリケーションの各ハッシュ照合に成功した。
- 起動診断はSD=1、GNSS=1、通信側BNO=1、ログキューdrop=0で、制御側UART受信カウンタは1,437→1,922と継続増加した。PCのWi-Fi 2は `XIAO-BOAT-TELEMETRY`（192.168.4.2）へ接続済みで、通信側（192.168.4.1）へのpingは1〜5 msだった。
- `/api/estimated-state` をSoftAPへ直接要求し、制御側→UART→通信側→Web APIの全経路を確認した。応答時のフレーム年齢は3 ms、ジャイロ8.382 ms、加速度1.673 ms、GNSS7.123 ms、ToF79.809 msで、ToF/GNSSの健全性はVALIDであった。取付未較正のため姿勢・ヨーは意図どおりDEGRADED、アクチュエータ出力は未使用である。
- 同じ応答で `mag_age_us=4294967295` を確認した。これは取付未較正による意図的なヨー補正停止ではなく、通常環境（BOAT_EXPERIMENT=0）がGame Rotation Vectorを有効化して地磁気レポートを有効化していなかった実装不足である。通常DRY_RUNを加速度・較正ジャイロ・較正地磁気に修正した。制御側を再接続して修正版を書込むまで、ヨーは有効化しない。
- 修正版の制御側 `seeed_xiao_esp32s3` ビルドは成功した（RAM 109,700 bytes / 33.5%、Flash 547,149 bytes / 16.4%）。この時点でUSB COMポートとして接続されているのは通信側COM4だけであるため、制御側の修正版書込みと `mag_age_us < 120000` のライブ確認は保留した。

## 2026-07-26 -- 推定・航法実装順の補強

- ユーザー提供の計画レビューを反映した。最優先ゴールを単なる地磁気鮮度確認から、「主副IMUの生データを機体座標・測定時刻・受信時刻・通信側ログ投入時刻とともに再生可能に保存する」ことへ明確化した。
- 軸較正の前にP1生ログを整備し、静止6姿勢に加えて機体X/Y/Z軸の正逆回転を記録する。取付変換はログで求め、再生で検証する。主副IMU比較は警告のみとし、自動切替や制御入力には用いない。
- ESKFはIMU予測（ESKF-0）、GNSS更新/NIS（ESKF-1）、遅延補償（ESKF-2）の順に分割する。各段階で再生可能な既知運動・GNSS欠落・外れ値試験を通過するまで、閉ループ制御・航法・実出力へ進まない。

## 2026-07-26 -- 制御側地磁気修正版を書込み、ライブ鮮度を確認

- 制御側をCOM5、MAC `34:85:18:AB:FA:90` と照合し、加速度・較正ジャイロ・較正地磁気を有効化した `seeed_xiao_esp32s3` 環境を書込んだ。ブートローダ、パーティション、アプリケーション547,520 bytesの各領域でesptoolハッシュ照合が成功した。
- 通信側SoftAPの `/api/estimated-state` は制御側の新しい推定フレームを返した。単発確認では地磁気年齢5,490 us、10回の連続確認では50〜33,447 usであり、以前の未受信値（4,294,967,295 us）は解消した。ジャイロ50〜17,681 us、加速度65〜12,983 us、GNSS21,237〜40,807 usも鮮度基準内だった。
- 取付変換は未較正のままなので、attitude/yaw healthは意図どおりDEGRADED、地磁気補正はfalse、アクチュエータ出力はDRY_RUNである。ToF年齢は79,630〜220,253 usと変動し、100 ms超ではheight healthがINVALIDになった。ToF単独の不良で姿勢・航法を停止しない縮退設計が動作している。
- 通信側の以前のUSB COM4診断ではSD=1、GNSS=1、BNO=1、ログdrop=0、制御UARTカウンタ増加を確認済みである。今回の最終確認時にはCOM4 USBがWindowsから消えていたため、そのシリアル再確認はできなかったが、SoftAP APIは連続して応答し、UART由来の推定フレームを受信していた。次は通信側USB再接続後にP1三時刻生ログを実装し、磁気3軸・accuracy・更新最大間隔・dropを含む完全な品質確認を行う。

## 2026-07-26 -- P1主副IMU三時刻生ログを実装

## 2026-07-26 -- 通信側P1対応版の書込みとP1スモーク試験

- 静止姿勢採取用に`/api/p1/start?confirm=1`でRUN0002を開始した。10秒時点ではrecords=5,757、drop=0、SD error=0、後続確認ではrecords=115,414、drop=0、SD error=0でP1は稼働していた。しかし次の確認ではcapture=false、logging=false、run=none、records=0となっていた。
- `POST /api/p1/stop` は実行しておらず、コードにもP1の時間自動停止条件はない。`/P1/RUN0002.TXT`およびBINのダウンロードはともにfile not foundで、`/api/manual`はSD ready、logging=false、run=none、fault=noneを返した。よってRUN0002は通信側の再起動または同等の状態初期化によりSD確定前に失われたと判断する。SD書込みエラーやキュードロップの正常終了ではない。
- RUN0002を較正ログとして採用しない。次の採取前に再起動理由とP1異常終了を保存する診断を追加し、継続記録の正常停止を再検証する。
- 診断版`0.3.6-p1-recovery`を通信側COM4/MAC `E0:72:A1:FC:08:D0`へ書込み、全領域ハッシュ照合に成功した。P1 APIにboot ID、リセット理由、前回P1セッションの回復状態を追加し、P1開始時の`/P1/ACTIVE.TXT`ジャーナルと、異常起動時の`/P1/RECOVERY.TXT`を実装した。ビルドはRAM 190,188 bytes（58.0%）、Flash 873,745 bytes（26.1%）で成功した。
- 更新後のP1開始は2秒後もcapture=false、logging=false、run=noneだった。通常ログの開始・停止でも`/api/manual`は`sd="error"`、logging=false、run=noneを返した。現在の直接原因はP1制御ではなく、通信側がSDカードを利用可能と判定していないことである。SDを挿し直して通信側を再起動し、SD readyを確認するまで新しいP1採取を行わない。
- SDの挿し直しと通信側再起動後、`/api/manual`はsd=ready、link_healthy=true、dry_run=trueへ復帰した。再起動理由は`reset_reason=1`（電源投入）で、P1異常回復マーカーはfalseだった。
- 復旧後の10秒P1確認は`/P1/RUN0003.BIN/TXT`でnormal_stop=1、records=5,368、queue_drops=0、queue_high_water=16、SD write error=0、BNO decode error=0、BNOイベントキュードロップ=0として確定した。SD最大書込み時間16.000 ms、最大ログキュー待ち23.013 msで、今回の短時間確認中に記録経路の異常はない。RUN0003は復旧確認用であり、取付変換の較正値には使用しない。
- 第1静止姿勢の本番P1としてRUN0004を開始し、12秒時点でcapture=true、records=6,453、queue_drops=0、SD error=0を確認した。その後、通信側のboot IDが`2859305578`から`3406201269`へ変化し、reset_reason=1（電源投入リセット）、capture=false、run=noneとなった。`/api/manual`はSD error、RUN0004.TXTはfile not foundであり、正常停止・SD確定に到達していない。
- この事象はP1停止APIやSD書込みエラーの処理ではなく、通信側の電源断またはUSB/SD物理接触喪失と一致する。RUN0004を較正データとして採用しない。以後のP1再開条件は、通信側のUSB給電・ケーブル固定・SDの確実な装着後に再起動し、SD readyが継続することとする。
- ユーザーの実機条件として、姿勢変更時に通信側USBを完全固定することはできない。このため、以後の静止6姿勢は各姿勢を独立した短時間P1 RUNとして採取し、停止・TXT確定後にだけ姿勢を変更する手順へ変更した。姿勢変更中の再起動は未確定RUNだけを無効とし、確定済みの姿勢ログを保全する。
- 通信側を再接続・再起動後、SD ready、control link healthy、DRY_RUN=trueを確認した。P1診断は`recovery_detected=true`、`recovered_run="RUN0004.BIN"`を返し、前回の電源投入リセットで未確定になったP1セッションを検出できた。P1ジャーナル／回復診断は実機で機能している。
- 静止姿勢1を独立P1として6秒間採取し、`/P1/RUN0005.BIN/TXT`をnormal_stop=1で確定した。records=3,212、queue_drops=0、queue_high_water=15、SD write errors=0、BNO decode error=0、BNO event-queue drops=0、SD最大書込み18.090 ms、最大ログキュー待ち27.430 msだった。通信側boot IDは採取中に不変であり、RUN0005を静止姿勢1候補として保全する。
- 静止姿勢2の初回`RUN0006.BIN/TXT`はnormal_stop=1、records=3,248、drop=0、SD error=0、BNO decode/event-queue drop=0であった。ただしSD最大書込み76.908 ms、最大ログキュー待ち121.089 ms、TimingDiagnostic=26であり、較正候補としては採用せず参考扱いとする。
- 同じ静止姿勢2を3秒間再採取した`RUN0007.BIN/TXT`はnormal_stop=1、records=1,613、queue_drops=0、queue_high_water=13、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、SD最大書込み11.750 ms、最大ログキュー待ち21.421 msだった。RUN0007を静止姿勢2候補として保全する。
- 静止姿勢3としてRUN0008のP1開始を要求したが、HTTPはタイムアウトし、`/api/p1`と停止要求に応答しなかった。COM4診断では`LOG started: /P1/RUN0008.BIN`の後に`I2C address not found`、BNO=0を観測した。RUN0008の正常停止・TXT確定を確認できないため無効とする。通信側を再起動して未確定RUNを破棄し、Web応答停止と通信側BNO I2C検出失敗を解消してから再試験する。
- 通信側を再起動後、SD ready、logging=false、control link healthy、DRY_RUN=true、Web API応答を確認した。`/api/sensors`は通信側BNO ready、fault=none、reinit=0、加速度・ジャイロ・地磁気が更新中、decode error=0、event queue drop=0を返した。RUN0008開始時のI2C検出失敗は継続していないが、姿勢3は未採取のままとする。
- 姿勢3の再採取要求時、SoftAPへの4回のHTTP要求はすべて接続前に失敗し、Wi-Fi 2はdisconnectedだった。P1開始要求は通信側へ届いておらず、RUN0009は作成していない。USBを完全固定せず姿勢変更する条件を反映し、以後は姿勢変更中は通信側USBを外し、姿勢決定後に再接続・起動確認してから短時間の独立P1を採取する手順へ変更した。
- 姿勢3を決めた後に通信側を再接続し、SD ready、control link healthy、DRY_RUN=true、通信側BNO ready（fault=none、decode/event-queue drop=0）を確認した。独立P1の`RUN0009.BIN/TXT`を2秒でnormal_stop=1として確定し、records=1,058、queue_drops=0、queue_high_water=14、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、SD最大書込み11.925 ms、最大ログキュー待ち22.874 msだった。RUN0009を静止姿勢3候補として保全する。
- 姿勢4ではSoftAPへの再接続要求は成功したが、14秒以上待機しても`192.168.4.1`のHTTP APIへ接続できなかった。P1開始要求は送らず、RUNは作成していない。通信側USBを抜き差しして通常起動・API応答を回復させてから姿勢4を再試行する。
- 姿勢4では通信側再起動後にSD ready、control link healthy、DRY_RUN=true、通信側BNO ready（fault=none、decode/event-queue drop=0）を確認した。独立P1の`RUN0010.BIN/TXT`を2秒でnormal_stop=1として確定し、records=1,056、queue_drops=0、queue_high_water=14、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、SD最大書込み12.184 ms、最大ログキュー待ち20.543 msだった。RUN0010を静止姿勢4候補として保全する。
- 姿勢5では通信側再接続後にSD ready、control link healthy、DRY_RUN=true、通信側BNO ready（fault=none、decode/event-queue drop=0）を確認した。独立P1の`RUN0011.BIN/TXT`を2秒でnormal_stop=1として確定し、records=1,055、queue_drops=0、queue_high_water=12、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、SD最大書込み11.925 ms、最大ログキュー待ち22.677 msだった。RUN0011を静止姿勢5候補として保全する。
- 姿勢6では通信側再接続後にSD ready、control link healthy、DRY_RUN=true、通信側BNO ready（fault=none、decode/event-queue drop=0）を確認した。独立P1の`RUN0012.BIN/TXT`を2秒でnormal_stop=1として確定し、records=1,045、queue_drops=0、queue_high_water=14、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、SD最大書込み11.948 ms、最大ログキュー待ち21.996 msだった。RUN0012を静止姿勢6候補として保全する。
- RUN0005/0007/0009/0010/0011/0012のBINを完全解析した。制御側／通信側とも静止加速度の標準偏差は各軸0.01〜0.08 m/s²で、静止区間として十分だった。通信側平均加速度[m/s²]は順に(-3.080,0.807,-9.401)、(-1.562,-0.059,-9.822)、(7.195,1.500,-6.646)、(2.546,-7.683,-5.876)、(1.666,0.849,-9.803)、(0.291,-1.228,-9.893)。正X寄り・負Y寄りの2姿勢は得られたが、残り4本は負Z寄りに集中している。よって6本を取付変換算出にはまだ使用せず、反対方向を含む大きく異なる傾きの独立RUNを追加採取する。
- ユーザーの実機制約により追加姿勢は要求しない。6本の主副IMU加速度方向ペアでWahba回転推定を行い、通信側→制御側の暫定相対回転quaternion `(0.99969,-0.00269,0.01881,-0.01631)` を得た。方向のGram固有値は4.6795/0.7415/0.5790（条件数8.08）であり、完全な一方向集中ではない。加速度方向残差は平均4.683°、最大6.528°だった。
- この推定値は主副IMUの比較整列用の暫定値に限定する。地磁気干渉・機体軸の既知基準・ヨー軸の独立検証がないため、機体座標への取付変換、`kBnoMountValidated`、姿勢／ヨーのVALID化、アクチュエータ制御には用いない。
- 暫定相対回転を用いる`DUAL_IMU_COMPARE`を実装した。共通プロトコルにType 24 `PrimaryImuSnapshot`を追加し、制御側は主IMUの加速度・ジャイロ・地磁気と各センサ時刻を20 Hzで送る。通信側は副IMUを暫定行列で制御側座標へ変換し、`/api/dual-imu`および`/dual-imu`で差分ノルムと鮮度を表示する。APIは`provisional=true`と`comparison_only=true`を返し、正式な機体軸変換や制御へ使わない。
- ビルド: 制御側RAM 109,772 bytes（33.5%）/ Flash 547,517 bytes（16.4%）、通信側RAM 190,264 bytes（58.1%）/ Flash 876,957 bytes（26.2%）で成功した。通信側COM4/MAC `E0:72:A1:FC:08:D0`へ書込み、全領域ハッシュ照合が成功した。`/api/dual-imu`は応答し、制御側が旧ファームウェアのため`available=false`、通信側IMU鮮度は5/10/16 msを返した。制御側USB接続後にType 24の実機受信と比較値を確認する。
- 制御側COM5/MAC `34:85:18:AB:FA:90`へType 24対応版を書込み、ブートローダ、パーティション、アプリケーション547,888 bytesの全領域ハッシュ照合に成功した。起動後の`/api/dual-imu`はavailable=true、provisional=true、comparison_only=true、主IMU age=11 ms、副IMU accel/gyro/mag age=4/10/26 ms、差分ノルム accel=1.8876 m/s²、gyro=0.0034 rad/s、magnetic=12.2390 µTを返した。同時にSD ready、control link healthy、DRY_RUN=trueを確認した。
- この確認は制御側→UART→通信側→Web APIの主副IMU比較経路が成立したことを示す。暫定変換の残差（平均4.683°／最大6.528°）はAPIに明示し、機体軸変換、姿勢/YawのVALID化、アクチュエータ出力へ使用していない。
- SoftAPへ再接続後にRUN0001.BIN（153,444 bytes）を完全解析した。1,671レコードが先頭から末尾まで整合し、主BNOである制御側起動ID `41526774` は加速度185・ジャイロ146・地磁気73、通信側起動ID `1686839828` は加速度365・ジャイロ293・地磁気74を記録した。主副のBNOストリームが同一BIN内で識別できることを確認した。
- 全1,136件のBNOレコードはpayload長56 bytesであり、各payloadの`sensorUs`、`callbackUs`、`queuePushUs`と、BIN外側の`logQueueUs`はすべて非ゼロだった。P1で必要な三時刻と通信側ログ投入時刻は欠落なく保存されている。

- 通信側をCOM4、MAC `E0:72:A1:FC:08:D0` と照合し、P1 API・Web UI・SD記録を含む `seeed_xiao_esp32s3` イメージを書込んだ。ブートローダ、パーティション、アプリケーション872,864 bytesの全領域でesptoolハッシュ照合に成功した。
- SoftAP経由で `POST /api/p1/start?confirm=1`、約3秒後に `POST /api/p1/stop` を実行した。`/P1/RUN0001.BIN` は正常停止として確定し、records=1,671、queue_drops=0、sd_write_errors=0であった。SD書込み高水位は29、最大書込み時間は10.890 msだった。
- 同RUNのTXTで、通信側BNOは加速度405、ジャイロ324、地磁気81イベント、BNO decode error=0、BNOイベントキュードロップ=0を確認した。P1の開始・停止・SD確定および副BNO生ログの経路は成立している。
- 書込み直後の再起動でPCのSoftAP接続が切れたため、BINを再ダウンロードして主BNO・副BNOのレコードを個別集計する確認は保留した。これは主BNOの記録成立を未検証のまま残すものであり、静止6姿勢や軸回転の較正ログとしてRUN0001を使用しない。再接続後にまずBINの型別・送信元別集計を行う。

- 既存のBNO payloadはセンサ測定時刻、SH-2コールバック時刻、BNOイベントキュー投入時刻を保持し、通信側BINの外側時刻はログキュー投入時刻であることを確認した。これをP1の三時刻ログとして採用し、通信遅延とセンサ差を混同しない。
- 新しい `P1Capture` プロトコル（Type 23）を追加した。通信側の `POST /api/p1/start?confirm=1` は `/P1/RUNxxxx.BIN` を開いた後に主BNO生ストリームを有効化し、`POST /api/p1/stop` はストリーム停止後にSDを確定する。`/p1` と `GET /api/p1` はDRY_RUN状態、RUN名、件数、キュードロップ、SDエラーを表示する。
- 通信側の副BNOは既存どおりログ中に全生イベントを記録する。制御側はP1中だけ加速度・較正ジャイロ・較正地磁気を送信し、通常時のUART負荷とSTOP/E-STOP経路を維持する。P1開始・停止はアクチュエータ出力を変更しない。
- 検証: 制御側ビルド成功（RAM 109,700 bytes / 33.5%、Flash 547,237 bytes / 16.4%）、通信側ビルド成功（RAM 190,164 bytes / 58.0%、Flash 872,497 bytes / 26.1%）。制御側COM5/MAC `34:85:18:AB:FA:90`へ書込み、アプリケーション547,600 bytesを含む全領域のハッシュ照合に成功した。通信側はUSB未接続のため未書込み、P1の実機開始・BIN解析は未実施である。
