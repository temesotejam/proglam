# BNO08X姿勢軸確認試験 RUN0059（2026-08-03）

## 試験条件

- 対象：ボート用XIAO COM4、ボート用CoreS3 COM6
- COM3：未接続・未書込み・未操作
- XIAO設定：`BOAT_EXPERIMENT=21`（Gyroscope 100 Hz、Game Rotation Vector 100 Hz）
- 開始API：`POST /api/log/start?duration_s=60&bno_capture=1&bno_log_enqueue=1&uart_rx_diag=dispatch&eskf_reset_at_start=1`
- CoreS3シリアル：開かず、Web APIのみ使用
- `shadow_only=true`、`actuator_output_enabled=false`

## 保存物

- `pc-tools/boat_eskf/captures/BNO_ATTITUDE_AXIS_60S_20260803/start_response.json`
- `files_after.json`
- `RUN0059.BIN`、`RUN0059.TXT`
- `link_after.json`、`eskf_after.json`
- `run0059_analysis.json`

## 保存・finalize結果

`RUN0059.TXT`より、`normal_stop=1`、`records=17721`、`queue_drops=0`、`queue_high_water=33`、`sd_write_errors=0`、`control_crc_errors=0`、`control_cobs_errors=0`、`control_length_errors=0`。TXT生成も完了しています。BINは1,980,306 bytes、17,721 records、trailing byte 0でした。

BIN内の主なraw件数はGyroscope（type 3）5,888件、Game Rotation Vector（type 4）5,888件です。callback timestamp spanは約60.005秒で、実効周期は約98.13 Hzです。四元数ノルムは平均0.999994、最小0.999958、最大1.000043でした。

## 通信・シーケンス

終了時Core APIはUART sequence gap=0、CRC/COBS/length error=0、time-sync timeout=0、link connectedでした。BINの各raw recordについてcallback timestampは単調増加（非増加0）です。一方、SH-2由来のsensor timestampは32 bit wrapを含み、非増加はGyro 3,707、GVR 3,316でした。

raw report sequenceは、Gyro/GVRとも同じ位置で1回だけ不連続でした。約1.15秒のcallback間隔が発生し、report sequenceがそれぞれ18→0へ戻っています。これは試験中にBNO08X側のreport再初期化またはリセットが発生した可能性を示しますが、Coreシリアルを開いていないため原因は未確定です。したがって、通信・SD経路は合格ですが、姿勢軸試験全体は「部分成立」と判定します。

## 姿勢変化（callback timestampの5秒区間平均）

Euler角は±180度境界をまたぐため、平均値だけで符号を断定しません。0–25秒は基準姿勢でほぼ一定でした。その後は次のような大きな変化が観測されました。

| 区間 | Roll平均 (deg) | Pitch平均 (deg) | Yaw平均 (deg) | 備考 |
|---|---:|---:|---:|---|
| 0–5 s | -176.18 | 1.92 | 171.35 | 基準姿勢 |
| 20–25 s | -176.16 | 1.92 | 171.35 | 基準姿勢 |
| 25–30 s | 境界通過 | 3.86 | 170.81 | 回転遷移 |
| 30–35 s | -123.25 | 75.61 | -125.28 | 大きな姿勢変化 |
| 35–40 s | -146.50 | 56.14 | -138.31 | 保持・遷移 |
| 40–45 s | -153.45 | -78.48 | 125.55 | Pitch側の大変化 |
| 45–50 s | -166.50 | -60.54 | 境界通過 | Yaw境界通過 |
| 50–55 s | 境界通過 | 1.52 | -160.80 | Roll側の大変化 |
| 55–60 s | 約94.80 | 8.26 | -166.98 | 終了姿勢 |

この結果から、Roll/Pitch/Yawのいずれにも反応があることは確認できます。ただし、操作開始が予定時刻からずれており、report reset区間とEuler角のラップも重なっているため、各軸の正負符号をこの1回だけで確定しません。

## Core ESKF状態

終了時APIは`health_reason=imu_stale`、`run_state=0`、`eskf_initialized=false`、`q_nb=[1,0,0,0]`でした。これは今回がGVR raw取得用の一時設定で、通常ESKF運用周期ではないためです。通信異常やアクチュエータ出力の発生を示すものではありません。

## 次の判断

1. 同じ設定で、各軸を一つずつ、開始時刻を合わせて短い区間で再試験する。
2. BNO report sequence resetが再発するかを確認する。
3. 軸確認完了後、XIAOを`BOAT_EXPERIMENT=23`へ戻す。

