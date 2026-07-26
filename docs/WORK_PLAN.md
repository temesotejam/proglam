## 2026-07-26 -- RUN0020を正式な比較基準に確定

- RUN0020を、通信側 `0.3.5-ingress-timestamp`・直結10 cm・DRY_RUNにおける安定動作の基準構成とする。RUN0013以前のBOAT24不合格結果は旧実装の診断記録として保持するが、現行構成の判定には用いない。
- 再接続時は構成を変更せず、短時間のBNO／GNSS／制御UART／SD再現確認から開始する。BOAT23への移行は別途ユーザー指示を待つ。

## 2026-07-26 -- BOAT24 RUN0020（記録間隔ゲート合格）

- 通信側ログを専用タスクへ分離し、BINの外側タイムスタンプをSDタスクの書込み開始時刻ではなく、フレームが記録キューへ入った時刻に変更した。SD待ち時間は従来どおり `sdTaskUs` と TimingDiagnostic に残すため、記録時刻とストレージ遅延を混同しない。
- 直結10 cm・DRY_RUNで `RUN0020.BIN/TXT` を60.039 s測定した。通信側／制御側の加速度・ジャイロ・地磁気すべてでsequence欠落、SD書込みエラー、ログキュードロップ、BNOデコードエラー、BNOイベントキュードロップ、結果CRCエラーは0である。地磁気の最大記録間隔は通信側46.989 ms、制御側UART由来53.575 msで、80 ms以下を満たした。
- SDタスク待ちの最大値123.600 msとTimingDiagnostic 197件は診断として残るが、ログキュー高水位80/160で吸収され、欠落はない。BOAT24の記録間隔ゲートは通過した。BOAT23はユーザー指示があるまで開始しない。

## 2026-07-26 -- BOAT24 RUN0018（記録間隔ゲート不合格）

- 10 cm直結・DRY_RUN・固定条件で `RUN0018.BIN/TXT` を取得した。60.056 sの測定は正常終了し、SD書込みエラー、ログキュードロップ、I2Cエラー、BNOデコードエラー、BNOイベントキュードロップ、結果CRCエラーはすべて0、BNOの各ストリームにもsequence欠落はない。
- ただし地磁気の最大記録間隔は通信側158.076 ms、制御側UART由来164.476 msであり、要求する80 ms以下を満たさない。最大ログキュー待ちは124.516 ms、TimingDiagnosticは96件である。最大SD書込み時間16.849 msを超える待ちであり、次は通信側のログキュー／SDタスクのスケジューリング遅延を特定・低減する。BOAT23は開始しない。

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
| 12 | 状態推定、ToF評価、ロール、舵、VESC、航法 | IN PROGRESS | 状態推定のDRY_RUN実装は完了。取付軸較正、実機再現、ToF評価、アクチュエータ出力は未着手 |

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

## 2026-07-26 -- 状態推定の初期DRY_RUN実装

- 状態: **IN PROGRESS（ビルド済み・実機未書込み）**。RUN0020を通信と取得周期の比較基準として固定し、制御側BNO08Xの生加速度・生ジャイロをセンサ時刻で処理する四元数推定器を追加した。
- 安全条件: BNOの機体軸変換は未較正である。設定は恒等変換の仮置き、`kBnoMountValidated=false` とし、推定健全性は意図的に `DEGRADED` とする。推定値をサーボ・VESCその他の出力へ接続しない。
- 通信側は制御側の推定状態を100 ms周期で受信し、SoftAPの `/state` と `/api/estimated-state` で、姿勢、角速度、入力鮮度、健全性、補正状態を表示する。
- 次: 両XIAO再接続後、現構成のまま書込み、短時間DRY_RUNでUART/SD/センサ鮮度を確認する。続いて静止6姿勢と既知ヨー回転でBNO取付変換を実測し、`kBnoMountValidated` を有効化する。その検証前に制御出力は実装しない。

### 書込み状況（2026-07-26）

- 制御側XIAO: COM5、MAC `34:85:18:AB:FA:90` を読出しで照合後、`seeed_xiao_esp32s3` 環境の `0.3.5-estimated-state-dry-run` を書込み、esptoolの各領域ハッシュ照合を通過した。さらにアプリケーション領域547,504 bytesを目的バイナリと読み取り照合し、digest一致を確認した。起動後4秒の診断で、`dry=1`、BNO=1、ToFフレーム増加、INAエラー0、NAV/結果とも10 Hz・CRC/連番ギャップ0を確認した。
- 通信側XIAO: USB COMポートは未検出であり、状態表示/APIを含む新ファームウェアは未書込み。USB接続後にMACを照合してから同じく明示環境で書込む。

### 地磁気入力の修正待ち（2026-07-26）

- 初回の `/api/estimated-state` ライブ確認で、加速度・ジャイロ・ToF・GNSSは鮮度基準内だった一方、`mag_age_us` は未受信値であった。通常環境（BOAT_EXPERIMENT=0）が地磁気レポートを有効化していないことを確認した。
- 通常DRY_RUNを設計どおり加速度・較正ジャイロ・較正地磁気（20 ms / 20 ms / 50 ms）に修正し、制御側 `seeed_xiao_esp32s3` 環境でビルド成功（RAM 33.5%、Flash 16.4%）を確認した。制御側をUSB再接続して再書込み後、`mag_age_us < 120000` を確認するまでヨーの状態は `DEGRADED` のままとする。

## 2026-07-26 -- 推定・航法計画の具体化

- RUN0020はログ番号だけでなく、両ノードのソース、ビルド済みバイナリ、環境・ライブラリ版、設定、プロトコル版、基板MAC、短時間の正常ログ、API応答例を含む**読み取り専用の再現基準一式**として保存する。以後の変更・試験はRUN0021以降として扱い、RUN0020基準を上書きしない。
- 地磁気修正版の合格は、`mag_age_us < 120000` だけで判断しない。約20 Hzの更新周期と最大間隔、3軸値のヨー回転時の連続変化、磁場強度、BNO accuracy、IMU/UART/SDのdrop増加なし、静止時の異常な方位ジャンプなしを確認する。地磁気は取付・干渉評価が終わるまで制御補正へ使わない。
- ESKFより先に、主副IMUの生加速度・角速度・地磁気、連番、qualityを、センサ測定時刻・制御側受信時刻・通信側ログ投入時刻の3種類とともにP1ログへ保存する。静止6姿勢と機体X/Y/Z軸の正逆回転を記録し、ログから各BNOの取付変換を決め、同じログの再生で確認する。
- `DUAL_IMU_COMPARE` は比較・警告専用とする。生3軸、姿勢クォータニオン差、回転角、時刻差、更新周期、age、連番欠落、accuracy、瞬間差・移動平均差・閾値超過時間を記録し、自動切替・制御入力・サーボ操作には使わない。
- ESKFは三段階に分ける。`ESKF-0` はIMU予測・静止健全性、`ESKF-1` はGNSS位置/速度更新と残差/NIS/採否記録、`ESKF-2` は測定時刻への更新と再伝播による遅延補償である。各段階は静止、既知ヨー回転、直進、GNSS欠落、外れ値挿入を含む再生試験で再現性・有限性・棄却動作を確認してから次へ進む。
- モード、3翼ミキシング、VESC、LOSはESKFの後にDRY_RUNで実装する。遷移表、STOP/E_STOP差、設定変更はDISARMEDのみ、全ミキシング途中値のP2ログ、VESC Duty 0の物理的安全確認を先に固定する。

### 地磁気修正版の書込み・基本確認（2026-07-26）

- 制御側COM5、MAC `34:85:18:AB:FA:90` を照合し、`seeed_xiao_esp32s3` 環境の地磁気有効版を書込んだ。ブートローダ、パーティション、547,520-byteアプリケーションの各領域でハッシュ照合に成功した。
- 通信側SoftAP APIの連続10サンプルで、制御側地磁気の年齢は50〜33,447 us、ジャイロは50〜17,681 us、加速度は65〜12,983 us、GNSSは21,237〜40,807 usであった。地磁気入力の未受信状態は解消した。取付未較正のため姿勢・ヨーは引き続きDEGRADEDであり、地磁気補正・アクチュエータ出力は無効である。
- ToF年齢は79,630〜220,253 usであり、100 msを超える期間は高さhealthがINVALIDとなった。これは高さだけを縮退させる設計どおりの表示で、姿勢・航法の有効性を上げる根拠にはしない。地磁気3軸値、accuracy、最大間隔、干渉、dropの完全な合格判定は、次のP1三時刻生ログで確認する。

### P1生IMUキャプチャ実装（2026-07-26）

### P1書込み・スモーク試験（2026-07-26）

- 2026-07-26の静止姿勢採取で`RUN0002.BIN`を開始したが、記録中に通信側のP1状態が初期化された。`/api/p1`と`/api/manual`はlogging=false、run=none、records=0、SD error=0を返し、`/P1/RUN0002.BIN/TXT`は存在しなかった。P1はコード上で自動停止しないため、通信側の再起動または同等の状態初期化と扱う。このRUNを較正データとして使用しない。
- 追加切り分け: 通信側の更新後にP1・通常ログを開始しても、`/api/manual`は`sd="error"`、logging=false、run=noneを返した。現在はSDをマウントできず、新規RUNを作成できない。SDを挿し直し、通信側を再起動して`sd="ready"`を確認するまで、P1較正を開始しない。
- 復旧確認: SDの挿し直しと通信側再起動後、`/api/manual`はSD ready、制御リンクhealthy、DRY_RUN=trueを返した。P1の10秒確認は`/P1/RUN0003.BIN/TXT`をnormal_stop=1で確定し、records=5,368、queue_drops=0、SD write error=0、BNO decode/event-queue drop=0だった。次のP1は静止6姿勢の本番記録として開始できる。
- 本番採取失敗: 第1静止姿勢としてRUN0004を開始し、12秒時点でrecords=6,453、drop=0、SD error=0を確認した。しかし後続確認で通信側boot IDが変化し、`reset_reason=1`（電源投入リセット）、P1 inactive、run=noneとなった。再起動後の`/api/manual`もSD error、`/P1/RUN0004.TXT`はfile not foundであった。RUN0004は採用しない。通信側のUSB給電・ケーブル保持・SDカード接触を物理的に安定化し、再起動後にSD readyが継続することを確認するまでP1を再開しない。
- 採取方法を変更する。USBを完全固定できないため、静止6姿勢を一つの連続P1では取得しない。各姿勢でSD readyを確認してから3〜10秒だけ新規P1を開始し、**姿勢を動かす前に必ず正常停止・TXT確定を確認する**。姿勢変更中にUSBが動いて通信側が再起動しても、既に確定したRUNは失われない。各RUNの起動ID・reset reason・静止区間を後で対応付けて較正に使用する。
- 静止姿勢1: `RUN0005.BIN/TXT`を6秒でnormal_stop=1として確定した。records=3,212、queue_drops=0、SD write error=0、BNO decode/event-queue drop=0である。第1姿勢候補として保全し、姿勢変更後に次の独立RUNを採取する。
- 静止姿勢2: 初回`RUN0006`はnormal_stop=1・欠落0だが、SD最大書込み76.908 ms、最大ログキュー待ち121.089 ms、TimingDiagnostic 26件だったため参考扱いとする。同じ姿勢の再採取`RUN0007.BIN/TXT`はnormal_stop=1、records=1,613、drop=0、SD error=0、TimingDiagnostic=0、最大ログキュー待ち21.421 msで確定した。RUN0007を第2姿勢候補として保全する。
- 静止姿勢3: 再接続後にSD ready、BNO ready、制御リンク正常を確認してから、`RUN0009.BIN/TXT`を2秒でnormal_stop=1として確定した。records=1,058、queue_drops=0、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、最大ログキュー待ち22.874 msである。RUN0009を第3姿勢候補として保全する。
- 静止姿勢4: 再接続後にSD ready、BNO ready、制御リンク正常を確認してから、`RUN0010.BIN/TXT`を2秒でnormal_stop=1として確定した。records=1,056、queue_drops=0、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、最大ログキュー待ち20.543 msである。RUN0010を第4姿勢候補として保全する。
- 静止姿勢5: 再接続後にSD ready、BNO ready、制御リンク正常を確認してから、`RUN0011.BIN/TXT`を2秒でnormal_stop=1として確定した。records=1,055、queue_drops=0、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、最大ログキュー待ち22.677 msである。RUN0011を第5姿勢候補として保全する。
- 静止姿勢6: 再接続後にSD ready、BNO ready、制御リンク正常を確認してから、`RUN0012.BIN/TXT`を2秒でnormal_stop=1として確定した。records=1,045、queue_drops=0、SD error=0、BNO decode/event-queue drop=0、TimingDiagnostic=0、最大ログキュー待ち21.996 msである。
- 6候補BINの加速度平均・標準偏差を解析した。すべての主副IMUで軸ごとの標準偏差は0.01〜0.08 m/s²であり、静止性は良好だった。一方でRUN0005/0007/0011/0012は重力ベクトルが概ね負Z方向に集中し、RUN0009（正X寄り）とRUN0010（負Y寄り）を加えても、取付変換のための球面上の広がりは不足している。追加で、既存と異なる大きな傾き（目標: 正X寄りの反対、負Y寄りの反対、第三の斜め方向）の独立P1を最低3本採取してから取付変換を推定する。
- 実機制約により追加姿勢は採取しない。既存6本から主副IMU間の相対回転は暫定推定できた（通信側→制御側: quaternion `(0.99969,-0.00269,0.01881,-0.01631)`、加速度方向残差平均4.683°／最大6.528°、姿勢方向のGram条件数8.08）。これは`DUAL_IMU_COMPARE`の比較専用候補には使えるが、機体軸への取付変換・ヨー有効化・`kBnoMountValidated=true`の根拠にはしない。正式な機体軸変換は、固定治具または追加可能な既知姿勢が得られるまで未検証として保持する。
- `DUAL_IMU_COMPARE`を実装した。制御側は主IMUの加速度・ジャイロ・地磁気スナップショットを20 Hzで送信し、通信側は暫定相対回転で副IMUを制御側座標へ写して3種の差分ノルム・入力鮮度を`/api/dual-imu`と`/dual-imu`へ表示する。比較専用・provisionalをAPIで明示し、機体軸変換、推定器のVALID化、ヨー補正、アクチュエータ出力には接続しない。両方のビルドは成功し、通信側COM4へ書込み済み。制御側USB未接続のため、比較値の実機確認と制御側書込みは保留する。
- 制御側COM5/MAC `34:85:18:AB:FA:90`へ書込み、主副IMU比較を実機確認した。`/api/dual-imu`はavailable=true、provisional=true、comparison_only=true、主IMU鮮度11 ms、副IMU鮮度4/10/26 ms、加速度差1.8876 m/s²、ジャイロ差0.0034 rad/s、地磁気差12.2390 µTを返した。SD ready、制御リンクhealthy、DRY_RUN=trueである。これは比較経路の成立確認であり、正式な取付変換の合格や制御有効化ではない。

### 仮統合の全経路（機体固定前）

**2026-07-27更新:** 主副IMU影融合、GNSS/ToFゲート、DISARMED固定の仮想3翼/VESCミキサ、Type-25 SDログ、`/api/provisional-system` とWeb UIまで実装済み。通信側XIAO（COM4/MAC `E0:72:A1:FC:08:D0`）へ書込み、ライブAPIで主副IMU・ToF・出力ゼロ固定を確認済みである。GNSSは受信/fix待ちのため影航法入力は不採用表示である。制御側への実出力は引き続き禁止する。

**2026-07-27設計更新:** 添付の最新設計を以後の受入条件とする。次の実装順は、(1) `DISARMED/CALIBRATION/REPLAY` の出力ゼロ状態機械、(2) static-6face・各軸回転・gyro/mag/time-offset・ToF・servo/VESC用の較正RUNと統一ログ、(3) PC上のログ検証器とREPLAY、(4) 較正済みフラグにより停止可能なESKF shadow、(5) LOS・3翼/VESCのDRY_RUNである。`MANUAL/STABILIZE/AUTO` と実PWM/VESC書込みは、全較正・REPLAY・安全試験が合格するまで実装しても出力を有効化しない。

**2026-07-27実装:** `CALIBRATION` RUNの選択・開始・停止APIと画面を追加した。種別はstatic-6face、各軸回転、gyro bias、磁気、時間ずれ、ToF、servo geometry、VESC telemetryであり、開始／停止マーカーを含む `/CAL/RUNxxxx.BIN` を作る。通信側実機で待機APIが `DISARMED`、出力無効、SDエラー0を返すことを確認済み。次は機体固定後の `STATIC_6FACE` 実測と、PC側REPLAY/検証器である。

- 方針: 以下の全経路を実装・接続する。ただし全て `provisional` として明示し、実アクチュエータ出力は常にゼロ、正式推定・制御の入力には昇格しない。
- すでに成立: 主IMU推定（DEGRADED）、主副IMU比較、GNSS往復通信、ToF/INA取得、SD/P1ログ、SoftAP API、STOP/E-STOP、DRY_RUN。
- 今から仮接続する: (1) 主IMU基準＋副IMU条件付きの影融合姿勢、(2) GNSS位置/速度をゲートした影航法、(3) ToF距離をゲートした影高さ、(4) 影推定→モード→3翼ミキシング→VESC/サーボ指令の全ソフト経路。各段は値・鮮度・ゲート採否・理由をWeb APIとSDログへ出す。
- 現時点で作れない／正式化しない: 機体軸取付変換、磁気干渉補正とヨーのVALID化、GNSSの実航走精度、ToFの機体幾何と水面高さ、推進・舵・翼の実機効き、閉ループ航法。これらは機体固定後の正式較正・静止／回転／航走試験を待つ。
- 安全境界: 仮統合は比較・影推定・仮指令の生成までとし、PCA9685の実PWM、VESC Duty、操舵・翼出力は書き込まない。STOP/E-STOPとDISARMEDを全仮モードより優先する。
- 静止姿勢3の`RUN0008`はP1開始後にWeb APIが無応答となり、停止要求を処理できなかった。シリアル診断では`I2C address not found`と通信側BNO=0も観測した。RUN0008はSD確定を確認できないため採用しない。通信側を再起動して未確定RUNを無効化し、P1開始中にWeb操作が処理できなくなる原因を解消するまで姿勢3の再採取は行わない。
- USBを動かさずに姿勢を変えることは実機条件として不可能である。以後は、姿勢変更の前に通信側USBを外し、姿勢を決めてから通信側（必要なら制御側も）を再接続して起動・SD ready・BNO ready・SoftAP応答を確認する。その後に2〜3秒の独立P1を開始・停止・確定する。姿勢変更中に給電を維持しないので、未確定ログとWeb停止を避けられる。
- 次のP1較正採取の前に、通信側へ再起動理由とP1セッション異常終了を永続記録する診断を追加し、短時間ログと継続ログで正常停止・SD確定を確認する。根本原因が確認されるまで静止6姿勢の採取を進めない。
- 更新: RUN0001.BINを完全解析した。制御側BNO（起動ID `41526774`）は加速度185・ジャイロ146・地磁気73、通信側BNO（起動ID `1686839828`）は加速度365・ジャイロ293・地磁気74を記録した。BNO合計1,136件はすべて56-byte payloadで、`sensorUs`、`callbackUs`、`queuePushUs`、外側の`logQueueUs`がすべて非ゼロである。主副IMU三時刻ログの経路は成立した。
- 現在の次: 機体を動かさない静止6姿勢をP1で採取し、続けて機体X/Y/Z軸の正逆回転を記録する。RUN0001は短時間の配線・記録スモーク試験であり、取付変換の較正値としては使用しない。取付変換・ESKF・アクチュエータ出力へはまだ進まない。

- 状態: **IN PROGRESS（P1の開始・停止・SD確定を確認済み、取付軸較正は未実施）**。通信側COM4/MAC `E0:72:A1:FC:08:D0`へP1対応版を書込み、esptoolの全領域ハッシュ照合を通過した。
- `/api/p1/start?confirm=1` から約3秒後に `/api/p1/stop` を実行し、`/P1/RUN0001.BIN` を正常終了として確定した。記録数1,671、キュードロップ0、SD書込みエラー0である。副BNOは加速度405、ジャイロ324、地磁気81イベントを記録した。
- 次: SoftAPへ再接続した後、同BINを解析して主・副BNOの各ストリームと三時刻を個別に照合する。その後に静止6姿勢、機体X/Y/Z軸の正逆回転をP1で採取する。取付変換・ESKF・アクチュエータ出力へはまだ進まない。

- P1開始中だけ、通信側は`/P1/RUNxxxx.BIN`を開き、制御側へ `P1Capture` を送る。制御側は主BNOの加速度・較正ジャイロ・較正地磁気を送信し、通信側は副BNOと同じBINへ記録する。停止時は主BNO送信を停止してSDを確定する。通常時のBNO UART送信は従来どおり抑止し、STOP/E-STOP経路を圧迫しない。
- すべてのBNO payloadには`sensorUs`、`callbackUs`、`queuePushUs`があり、BIN外側時刻は通信側`logQueueUs`である。P1で要求するセンサ測定・制御側取得・通信側ログ投入の時刻を区別して再生できる。
- 制御側・通信側の`seeed_xiao_esp32s3`ビルドは成功した。制御側COM5/MAC `34:85:18:AB:FA:90`へP1対応版を書込み、全領域ハッシュ照合が成功した。通信側はUSB接続後に同版を書込むまでP1開始を行わない。
## 2026-07-27 -- 別PC引継ぎの固定化

- `docs/PC_HANDOFF.md` を正本ブランチ、確定コミット、別PCで再現できる範囲、PC側で別途必要な環境、検証済み到達点、未確定事項、実出力を有効化しない順序として追加した。
- 次の実作業は、機体固定後の正式校正ログ取得である。別PCへの移行後も、この順序を変更せず、校正値・ESKF・実出力を未検証のまま有効化しない。
