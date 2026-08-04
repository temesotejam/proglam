# MIN構成60秒SHADOW試験 事前確認結果（実機操作前停止）

## 結論

書き込み・実機試験は実施しなかった。現在のPR #18にはホスト側のMIN評価基盤と識別用feature flagはあるが、XIAO実機のMIN制御計算経路へ接続されていないため、指定された「MIN構成・全センサ・UART・SD・Web同時60秒試験」を開始できる状態ではない。

## 確認した基準

* 基準main: `f4e2908366ea139c8d55d2de71f4e2595be25438`
* ブランチ: `feat/proposal-benchmark-replay-20260804`
* 現在commit: `874e7acdad4c51d4445ce5f483c2db7343f02473`
* `BOAT_EXPERIMENT=23`: 維持
* PR #18: Draft、未マージ

## USB読み取り確認

WindowsのPnP情報だけを読み取った。シリアルポートは次の2つだけ検出された。

| ポート | USB VID:PID | 保存済み対応 |
| --- | --- | --- |
| COM4 | 303A:1001 | 制御側XIAO（MAC 34:85:18:AB:FA:90） |
| COM6 | 303A:1001 | 通信側CoreS3（MAC 30:ED:A0:D4:BF:40） |

COM3は検出されず、操作していない。今回の要求で必要な「通信側XIAO」を現在の接続で確定できない。

## ソース監査

* `xiao-boat-control-integration/platformio.ini`の`proposal_replay_min`は、`BOAT_EXPERIMENT=23`に`REPLAY_ENABLE=1`、`SHADOW_CONTROL_ENABLE=1`、`ACTUATOR_OUTPUT_ENABLE=0`、`PROPOSAL_PROFILE=1`を付けるだけである。
* `kProposalProfile`、`kBenchmarkEnable`、`kReplayEnable`は`app_config.h`で宣言されるが、制御側`src`のMIN計算経路では参照されていない。
* XIAO制御側`src`には、提言書で要求されたWaypoint、LOS、ILOS、COG判定、発進Yaw保持、Roll PD、翼合成等の実時間処理が存在しない。
* `pc-tools/boat_eskf/boat_eskf/proposal_benchmark.py`と`proposal_replay.py`はホスト評価用であり、ファームウェアへリンクされない。
* `xiao-boat-telemetry-integration`にはGNSS、SD、Web、UART、仮想SHADOW表示があるが、PR #18のMINフラグやMIN状態機械とは別の既存ファームウェアである。
* `kDryRunActuators=true`、`kActuatorOutputEnabled=false`は既存安全条件として確認できるが、新規`kProposalActuatorPathEnabled`を実際の全出力書込み箇所へ接続した状態ではない。

したがって、`proposal_replay_min`をCOM4またはCOM6へ書き込んでも、要求されたMIN実機試験にはならない。

## 実施していない操作

* COM4/COM6への書き込み
* シリアルポートを開く操作
* SDカード、GNSS、BNO08X、ToF、INA226、UART、Wi-Fi操作
* START、STOP、E-STOP、ログ開始
* アクチュエータ、ESC、VESC操作

## 再開に必要な選択

次のいずれかを確定する必要がある。

1. 通信側XIAO（GNSS/SD/Web実装済み）を接続し、MIN実時間経路を実装した上で試験する。
2. 現在の実機ペア（制御側XIAO + 通信側CoreS3）を正式な対象とし、既存の仮統合SHADOWをMIN相当として扱う範囲を明示的に承認する。

この選択が確定するまで、ファームウェア書き込みと60秒試験は行わない。A/B/C/D最終判定、MID/FULL、10分試験も保留する。
