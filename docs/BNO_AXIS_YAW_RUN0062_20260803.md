# BNO08X Yaw軸切り分け RUN0062（2026-08-03）

## 条件

- XIAO COM4：`BOAT_EXPERIMENT=21`（Gyroscope/GVR 100 Hz）を維持
- CoreS3 COM6：Web APIのみ。シリアルは開いていない
- COM3：未操作
- 20秒自動取得、`bno_capture=1`、`bno_log_enqueue=1`、`uart_rx_diag=dispatch`、`eskf_reset_at_start=1`
- 操作：基準姿勢 → Yaw +90°保持 → Yaw −90°保持 → 基準姿勢

## 保存結果

- BIN：`RUN0062.BIN`、679,374 bytes、6,147 records、trailing byte 0
- TXT：`RUN0062.TXT`、`normal_stop=1`
- Gyro/GVR：各2,002件
- queue drop：0、SD write error：0、queue high-water：34
- UART sequence gap、CRC、COBS、length error：0
- GVR四元数ノルム：平均0.999997、最小0.999958、最大1.000042

## 時刻・sequence

Gyro/GVRともcallback timestampは単調増加し、report sequence不連続は0でした。callback間隔は約10 msで、長時間欠測やreport sequenceリセットはありませんでした。SH-2 sensor timestampの非増加は32 bit wrapの影響です。

## 姿勢変化

5秒区間平均（Roll, Pitch, Yaw deg）は次のとおりです。

| 区間 | Roll | Pitch | Yaw |
|---|---:|---:|---:|
| 0–5 s 基準 | -176.74 | 0.38 | -173.77 |
| 5–10 s 回転保持 | -176.91 | 1.46 | 62.47 |
| 10–15 s 保持 | -131.85 | 1.86 | 97.41 |
| 15–20 s 復帰・遷移 | -58.99 | -0.70 | -84.19 |

Yaw軸の操作ではEuler Yawが約−174°から+97°、さらに−84°へ大きく変化しました。±180°境界を通過しているため、単純な平均・差分では符号を決めず、四元数と操作方向を併せて判定します。Yawは3軸の中で最も直接的にYaw成分へ現れました。

## 3軸の暫定対応

- 物理Roll操作：Euler Pitchが主に変化（RUN0060）
- 物理Pitch操作：Euler Rollが主に変化（RUN0061）
- 物理Yaw操作：Euler Yawが主に変化（RUN0062）

したがって、BNO取付姿勢またはX/Y軸の変換設定が未反映である可能性が高いです。今回の試験ではソフトウェアの軸変換値は変更していません。

## 通常運用へ復帰

3軸試験後、XIAO COM4へ`bno_accel100_gyro100_mag20_int_3min`（`BOAT_EXPERIMENT=23`）をビルド・書込みしました。ビルド成功、書込み時MACは`34:85:18:AB:FA:90`、Hash verifiedです。復帰後Core APIではPrimary IMU SnapshotとESKF Stateが新しいsequenceで受信され、`imu` ageは636 us、link sequence gap/CRC/COBS/lengthは0でした。ESKFは`run_state=2`、`health=1`、`health_reason=mount_unvalidated`、`shadow_only=true`、`actuator_output_enabled=false`です。`mount_unvalidated`は軸変換未確定を示す保留状態であり、通信停止ではありません。

## 判定

RUN0062はraw取得・通信・SD保存・report sequenceの観点で合格です。RUN0059のBNO report resetはRUN0060–0062では再発しませんでした。3軸の対応を記録し、通常運用周期へ復帰しました。

## 証跡

`pc-tools/boat_eskf/captures/BNO_AXIS_YAW_20S_20260803/`

