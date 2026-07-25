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