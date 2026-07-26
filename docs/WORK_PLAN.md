## 2026-07-26 -- BNO callback queues on both nodes (60-second screening pending)

- The control-side `bno::Reader` now replaces `getSensorEvent()` with the same direct SH-2 callback and 96-event FreeRTOS queue that passed on the communication node. D3 remains notification-only; BNO I2C and queue draining remain in the dedicated core-0 priority-3 task.
- Control BENCH events 29--32 now record callback count / BNO queue drops, queue high-water / decode errors, SH-2 service calls / maximum service time, and BNO UART enqueue successes / failures. This permits direct count matching across SH-2 callback → BNO queue → UART frame → communication receiver → log queue → SD.
- Added matching BOAT24 `bno_accel100_gyro100_mag20_int_60sec` environments (same 100 Hz accel, 100 Hz gyro, 20 Hz magnetic request; 60 s). Both images build successfully. The control image was uploaded to COM4 (MAC `34:85:18:AB:FA:90`) and boot diagnostics show `bno=1`, ToF updating, INA errors 0, UART NAV gaps 0.
- RUN0012 completed BOAT24 but is not an acceptance pass. The communication node has zero BNO sequence loss and zero queue/decode/SD/UART errors, but logged magnetic maximum gap is 200.252 ms (criterion <= 80 ms). On the control node, BNO pipeline diagnostics are loss-free but no magnetic frames were emitted. Root cause is a BOAT24 report-selection bug: `bno::Reader::enableReports()` enables the magnetic report only for BOAT23 and BOAT24 falls through to the legacy accel/gyro/game-rotation configuration. The control report condition is corrected and BOAT24 was uploaded to COM4 / MAC 34:85:18:AB:FA:90 with hash verification. Post-reset diagnostics show bno=1, ToF updating, INA errors 0, and NAV errors/gaps 0. Both nodes ran corrected BOAT24 in RUN0013. All BNO sequence, callback, queue, UART, logger, SD, ToF, INA, and I2C loss/error counters are zero and both magnetic streams are about 25 Hz, but logged magnetic maximum gaps are 111.384 ms (communication local) and 115.311 ms (control over UART), exceeding the <=80 ms gate. Timing instrumentation is now implemented and both BOAT24 images build successfully. Each BNO frame carries sensor/callback/queue-push time; recorder frames retain UART receive/log-queue/SD-task time; a fixed-size TimingDiagnostic BIN frame is emitted only when BNO log-queue wait is >=80 ms and includes all stages plus the last SD write start/end. Control diagnostic BOAT24 is uploaded to COM4 / MAC 34:85:18:AB:FA:90 with hash verification; boot diagnostics show bno=1, ToF updating, INA errors 0, and NAV errors/gaps 0. Communication diagnostic BOAT24 is uploaded to COM5 / MAC E0:72:A1:FC:08:D0 with hash verification; boot diagnostics show SD=1, GNSS=1, BNO=1, no idle logger drops, and increasing control-UART input. Both diagnostic images are ready. Next: repeat the 60-second BOAT24 and analyze TimingDiagnostic records; do not start BOAT23 yet.
## 2026-07-25 -- BNO08X magnetic-field acquisition (IN PROGRESS)

- Added BOAT_EXPERIMENT=23: accelerometer and calibrated gyroscope at 100 Hz, calibrated magnetic field at 20 Hz, D3 INT driven, 3 minutes.
- Both firmware images were uploaded and RUN0009 completed, but communication-side SD queue drops (1414) and BNO sequence gaps make the BOAT23 result a failed measurement. Diagnose logging throughput before retrying.
- RUN0010 confirms communication-side logging loss is now zero after the INT-task upload, but the BNO stream-rate and sequence-loss criteria remain unmet. Correct the communication-side BNO task scheduling before the next BOAT23 run.
﻿# 作業計画・現在地

## 2026-07-23 更新

通信・記録側の診断RUN0001で、周辺I2Cを100 kHzへ下げる計測は完了した一方、試験途中に400 kHzへ戻す再初期化で`prepare_timeout`となった。以後はI2C速度をRUN途中で変更せず、構成を固定した別RUNとして比較する。

最終更新: 2026-07-23
状態: `TODO`（未着手） / `IN PROGRESS`（進行中） / `BLOCKED`（外部条件待ち） / `DONE`（検証済み）

## 現在地

現在の主作業は、`xiao-boat-control-integration` と通信・記録側の二台を、実アクチュエータを動かさない固定構成で測定し、将来の制御周期を決める根拠を得ることである。診断フレームワークは`origin/agent/benchmark-stability`にあり、両XIAOを同一コミットで揃えてから試験する。

**次に行う作業:** INAは現行の低速・確実な監視設定を暫定採用し、詳細なP4比較は後回しにする。VESCテレメトリを優先し、DRY_RUN・Duty 0のまま3分間、VESC電圧、電流、eRPM、温度、故障コードの連続取得とUART完全性を確認する。速度推定はeRPMを駆動系の既知比とGNSS速度で較正して用いる。

## 実施順序

| 順序 | 作業 | 状態 | 完了条件・記録先 |
| --- | --- | --- | --- |
| 0 | プロジェクト文脈・Git管理・引継ぎ台帳を整備 | DONE | この3文書とルートREADME、AGENTS.mdに運用を記載 |
| 1 | 統合コードの差分レビュー | DONE（静的確認のみ） | `INTEGRATION_GAP_REVIEW.md` にピン、役割分離、プロトコル、ログ、計測指標、安全動作の差分を記録。ビルド・実機確認は未実施 |
| 2 | 通信・記録側の全体統合プロジェクト作成 | IN PROGRESS | 既存UART→SD基盤へGNSS、比較BNO、SoftAP/Web UIを統合。診断ブランチの同一コミットを両XIAOで使う |
| 3 | 制御側の最小受信と遠隔安全停止 | IN PROGRESS | STOP/E-STOP、Heartbeat、状態参照を通信側UARTから通し、P0で再確認する |
| 4 | 2台全体の縦切りスモーク試験 | IN PROGRESS | P0/P1で制御側全センサ、通信側GNSS/比較BNO、SD、Web UI、STOP/E-STOPを固定400 kHzで確認 |
| 5 | 制御側の受動統合計測（全センサ・出力なし） | IN PROGRESS | `EXPERIMENT_PLAN.md` のP1〜P7。センサ別Hz・最大間隔・I2C時間・UART・キュー・エラーを、1条件1RUNで記録 |
| 6 | 制御側 B: サーボ中央保持 | TODO | センサへのサーボ電源由来の干渉を比較 |
| 7 | 制御側 C: サーボ安全範囲の低速往復 | TODO | 急反転500/2500 µsは使用しない |
| 8 | 制御側 D: VESC状態取得（Duty 0） | TODO | UART応答、タイムアウト、センサ干渉を記録 |
| 9 | 制御側 E: VESC 3% | TODO | 低Duty基準での連続動作と安全停止を確認 |
| 10 | 制御側 F: サーボ低速往復 + VESC 3% | TODO | 電流変動とセンサ欠損の相関を記録 |
| 11 | G: 2台・SD・Web UIを含む全体試験 | TODO | 5〜10分、時刻同期、全ログ、全停止動作を確認 |
| 11 | 計測ログのBIN→CSV変換・周期/遅延/欠損解析 | TODO | フィルタ・制御周期の根拠を作成 |
| 12 | 状態推定、ToF評価、ロール、舵、VESC、航法 | TODO | 11の根拠がそろった後にこの順で着手 |

## 試験ごとの必須判定

各試験で、センサ更新停止なし、SD書込みエラー0、UART CRCエラー0、ログ／キュードロップ0、異常な最大間隔なし、STOP/E-STOP正常、正常停止サマリー生成を確認する。具体的な条件固定、実測項目、停止条件、合格後の判断は `EXPERIMENT_PLAN.md` を正本とする。満たせない場合は、`WORK_LOG.md` に事実・条件・次の切り分けを記録し、次段階へ進めない。


## 固定条件ファームウェア（2026-07-23）

- 状態: **IN PROGRESS（実機未検証）**。P0〜P7を個別にビルドする環境を追加済み。使用方法は [FIXED_EXPERIMENT_FIRMWARE.md](FIXED_EXPERIMENT_FIRMWARE.md) を参照。
- 次: P0を両XIAOへ書込み、SoftAP・SD・DRY_RUN・STOP/E-STOPを実機確認する。


### P0 実機書込み状況（2026-07-24）

- 通信側XIAO: p0_bringup_400k を COM5 へ書込み済み。起動後のシリアルで SD=1、GNSS=1、BNO=1、制御側UART受信を確認。
- 制御側XIAO: p0_bringup_400k を COM4 へ書込み済み。起動診断で `dry=1`、BNO08X正常、ToFフレーム増加、INAエラー0、UART NAV連番欠落0を確認した。一方、測定未開始時の状態は `FAULT`、`bench=0` であり、原因とP0全体の成立は未確認。
- P0自動測定: `E:\BENCH\RUN0009.BIN/TXT` は `benchmark_outcome=completed`、`normal_stop=1`、SD書込みエラー0、キュードロップ0、ログ異常なしで完走した。BINは27,046レコードを末尾まで復号でき、60秒の制御側結果もPASSだった。
- P0全体: **DONE**。RUN0009でWeb UI、SD、GNSS、Heartbeat、DRY_RUN、STOP、BIN/TXT回収を確認した。RUN0011の意図した `emergency_stop` ログに加え、制御側シリアル診断で `STATE E_STOP` と継続する `DIAG state=E_STOP` を直接確認した。E-STOP ACKはログ終了後のためBINには残らないが、制御側はE-STOP受信後にACKをキュー送信する実装である。

- 2026-07-24: 制御側XIAOを接続したが、Windows上でCOMポートおよびESP32/XIAO USBデバイスとして認識されなかった。再接続後はCOM4（USB Serial Device、VID:303A/PID:1001）として認識され、書込みに成功した。
- P1結果: RUN0015〜RUN0017の3反復で、連番欠落／重複0、SD書込みエラー0、キュードロップ0、ログ異常なし、安定区間GNSS往復完全性を確認。400 kHzの基準値はToF 7.214〜7.281 Hz、INA 7.300〜7.392 Hz、GNSS_NAV 10.000 Hz。ToF 10 Hz要求には未達であり、P3設定比較で改善可否を評価する。
- P2結果: RUN0018は測定フェーズのToF 0フレーム・GNSS結果2,132件欠落となり、RUN0019・RUN0020は同じ `prepare_timeout` で中断した。100 kHz固定はこの統合構成で不採用とし、共有I2Cは400 kHz固定を維持する。

## 2026-07-25 — BNO08X専用タスク化（進行中）

- 実装済み: 制御側のBNO08X取得・復旧を、ToF/INA/VESCを処理するメインループから専用FreeRTOSタスクへ分離した。BNOタスクはcore 0、優先度1、1 ms周期で動作し、UART送信タスク（core 0、優先度2）には譲る。
- 測定条件: `bno_attitude100_gyro50_3min`（BOAT_EXPERIMENT=20）。Game Rotation Vectorを100 Hz、較正ジャイロを50 Hzで要求し、ToF 4x4/30 Hz・INA現行設定・VESC Duty 0を維持する。3分間のログでBNO各出力率、欠番、最大サービス時間、ToF/INA/VESC/SD/UART健全性を確認する。
- 制御側: `bno_attitude100_gyro50_3min` をCOM4へ書込み済み（ハッシュ照合成功）。`IN PROGRESS`。
- RUN0004で姿勢100 Hz・ジャイロ50 Hz・ToF/INA/VESC各30 Hzの3分DRY_RUNを合格確認した。次はジャイロも100 Hzとする比較条件を準備・測定する。`IN PROGRESS`。
- BNOは制御・推定の中心として最高優先度の単独所有タスクにする。D3 INT通知駆動の自前推定用「加速度＋ジャイロ100 Hz」（BOAT22）はRUN0008でジャイロ欠番0・周辺系健全を確認した。加速度は実測126.489 Hzであり、以後の推定器入力は実測周期を用いるか100 Hzへ明示的に間引く。次はBNO内蔵推定用「ジャイロ＋姿勢100 Hz」を同じINT駆動で比較し、その後に共有I2C・VESC UARTの所有タスク分離を進める。
## 2026-07-26 -- RUN0014 BOAT24 timing diagnostic: failed (control-to-communication link timeout)

- RUN0014 is not an acceptance measurement: `normal_stop=0`, `benchmark_outcome=link_timeout`, and its complete recorder span is only 20.237 s rather than the required 60 s. Do not advance to BOAT23 from this run.
- The recorder, local communication BNO, and SD path remain healthy in the observed interval: BIN is fully parseable (8,403 frames, no tail), SD errors and logger drops are zero, BNO decode errors and BNO event-queue drops are zero, and the local stream is 124.5 Hz accel / 100.0 Hz gyro / 25.0 Hz magnetic with no observed sequence gap.
- The failure is specifically the control-to-communication transmit path. The control-origin BNO frames carry only 6.13 s of control-side creation timestamps but were received over 20.18 s; only 62 control heartbeats arrived while 147 communication heartbeats were sent. This is a control-link transmit backlog, not a BNO acquisition or UART receiver corruption problem.
- Root cause to address first: control `bnoTask` runs at priority 3 on core 0 and directly calls `linkSend()` through its BNO callback, while `linkTxTask` that drains the same FIFO is priority 2 on core 0. When the BNO INT line remains asserted, the BNO task has no blocking delay and can starve the FIFO-drain task. The saturated FIFO also delays/drops heartbeat frames, causing the communication node's measurement-time link watchdog to abort.
- TimingDiagnostic frames were correctly absent because `max_log_queue_wait_us=76,313`, below the current 80,000-us emission threshold. This does not satisfy the 80-ms inter-record criterion: local magnetic has a 103.418-ms recorded gap and a 70.510-ms frame-to-record delay. Lower the diagnostic emission threshold (recommended 60 ms) so near-limit waits are persisted with stage timestamps on the next run.

### Next actions (before repeating BOAT24)

1. Make control UART transmission schedulable while BNO is active: raise the FIFO-drain task above the BNO task and/or explicitly yield the BNO task for one tick after servicing. Preserve callback queue acquisition and verify its loss counters remain zero.
2. Preserve and report control-link FIFO high-water and drops even on an abort, so a subsequent `link_timeout` has a definitive counter record.
3. Reduce the TimingDiagnostic trigger from 80 ms to 60 ms; this is diagnostic-only and retains the 80-ms acceptance gate.
4. Rebuild, upload both BOAT24 images, then repeat a full 60-s run. Analyze all Type-21 timing records and require normal stop before judging the magnetic continuity gate.
## 2026-07-26 -- RUN0014 link-timeout mitigation implemented; control uploaded

- Control firmware `0.3.3-control-link-fairness`: the core-0 UART FIFO-drain task now has priority 4, above the priority-3 BNO task, and active BNO servicing explicitly delays for one tick after service. This keeps the callback queue design but prevents an asserted BNO INT from starving UART transmission and heartbeats.
- Communication firmware `0.3.3-bno-timing-60ms`: Type-21 diagnostic emission threshold is now 60 ms. The BOAT24 acceptance limit remains <=80 ms; the lower value only ensures near-limit waits are retained in BIN for analysis.
- Both BOAT24 builds succeeded: control RAM/flash 109,532 bytes (33.4%) / 539,749 bytes (16.1%); communication RAM/flash 190,028 bytes (58.0%) / 863,557 bytes (25.8%).
- Control image uploaded to COM4 / MAC `34:85:18:AB:FA:90`; esptool hash verification passed. Post-upload diagnostics show BNO=1, ToF incrementing, INA errors=0, and UART NAV TX/RX gap=0. Communication image is built but awaits COM5 upload.

### Next actions

1. Connect the communication node and upload `0.3.3-bno-timing-60ms`.
2. Run one complete 60-second BOAT24 measurement and require `normal_stop=1` before acceptance analysis.
3. Use the Type-21 records (now >=60 ms) and BNO continuity metrics to determine whether the remaining <=80-ms magnetic criterion is met.
## 2026-07-26 -- Communication diagnostic image uploaded; BOAT24 retry ready

- Uploaded communication firmware `0.3.3-bno-timing-60ms` to COM5 / MAC `E0:72:A1:FC:08:D0`; all esptool hash checks passed.
- Post-upload serial diagnostics: `SD=1`, `GNSS=1`, `BNO=1`, logger `drop=0`, and the control-UART receive counter increased. Both control and communication images are now deployed for the RUN0014 mitigation test.
- Next operation is exactly one complete 60-second BOAT24 DRY_RUN from the communication Web UI. Preserve the resulting BIN/TXT and do not advance to BOAT23 until it has `normal_stop=1`, no transport errors/drops, and the <=80-ms magnetic continuity gate is evaluated from the complete file.
## 2026-07-26 -- RUN0015/RUN0016 preflight failures: control STOP acknowledgement unavailable

- RUN0015 and RUN0016 are not BOAT24 measurement attempts. Both abort in preflight with `normal_stop=0`, `benchmark_outcome=stop_ack_timeout`, `command_ack_rx=0`, and no measurement phase. Their complete recorder spans are only 494.587 ms and 509.802 ms respectively.
- Both BIN files parse fully (172 / 150 frames) and the local recorder/BNO queues have zero errors, but this does not constitute a sensor or timing pass. No Type-21 records occur because neither run reaches load conditions.
- Each BIN contains the communication-origin Stop request and outgoing communication heartbeats, but no control-origin heartbeat or CommandAck. Only 3 / 6 control-origin ordinary data frames arrive. The immediate blocker is therefore the control-node response path or its power/UART connection at preflight, not an SD or BNO timing result.
- After analysis, Windows exposed no serial ports and COM4 could not be opened, so live control diagnostics cannot presently distinguish an unpowered/disconnected control board from a UART-link problem. Do not make another run until both nodes are powered and the control board is reachable again.

### Required connection check before retry

1. Power both XIAOs simultaneously and keep the control-to-communication UART connected (crossed TX/RX and common GND).
2. Confirm the control board re-enumerates in Windows, then verify its serial diagnostic shows BNO=1, ToF increasing, INA errors=0, and NAV TX/RX advancing.
3. Confirm the communication serial diagnostic has SD=1, GNSS=1, BNO=1, and a continuously increasing `control=` count before pressing the benchmark start button.
4. If STOP acknowledgement still times out with both boards confirmed, capture the two serial diagnostics before any further firmware change.
## 2026-07-26 -- Control connection restored after RUN0015/RUN0016

- Control COM4 re-enumerated and firmware `0.3.3-control-link-fairness` was observed after reset. Direct serial diagnostics show BNO=1, ToF increasing, INA errors=0.
- After reset, control `nav` and `gnss_result_tx` both advanced at 10 Hz with NAV error/gap=0. This confirms the communication-to-control UART direction is live again.
- The control remains `FAULT` while idle because of the normal no-host-heartbeat failsafe; benchmark preflight sends STOP and transitions it to DISARMED. The remaining check is the control-to-communication STOP acknowledgement during the next benchmark start.
## 2026-07-26 -- RUN0017 confirms preflight ACK starvation; control stream gate uploaded

- RUN0017 fully parses to 175 frames over 495.733 ms and again fails only as `stop_ack_timeout` with `command_ack_rx=0`. It contains the communication STOP request and four local heartbeats, but no control ACK or heartbeat; only three control BNO frames arrive.
- The control was observed to transition to DISARMED during the failed start, proving it received STOP. The missing item is its queued reply, not command reception. This confirms that start-before-measurement control BNO traffic was starving the reply in the shared control UART FIFO.
- Implemented and uploaded control firmware `0.3.4-control-bench-stream-gate` to COM4 / MAC `34:85:18:AB:FA:90`, hash verified. Control BNO acquisition continues continuously, but BNO UART frames are now emitted only while `bnoBenchmarkActive` is true (the actual measurement phase). STOP/ACK, Prepare/Ready, heartbeat, ToF, INA, and benchmark-result traffic are no longer preceded by idle BNO frames.
- Added control serial `link=used/drops/high-water` diagnostic. After upload and reset: BNO=1, ToF updating, INA errors=0, NAV TX/RX advancing at 10 Hz with no gaps, and idle link values settled at `0/0/2`. Thus the start-path FIFO is empty with zero drops before a benchmark.

### Next action

- Keep both nodes powered and run one BOAT24 retry. The primary first check is that STOP ACK now passes; retain the resulting BIN/TXT whether it completes or stops.