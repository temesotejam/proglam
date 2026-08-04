# 提言書評価ビルド比較（2026-08-04）

実機へ書き込まず、PlatformIOの`run`だけを実行した。全環境で`SUCCESS`を確認した。

## XIAO ESP32S3

| 環境 | ベース | BOAT_EXPERIMENT | RAM | Flash |
| --- | --- | ---: | ---: | ---: |
| `bno_accel100_gyro100_mag20_int_3min` | 既存 | 23 | 205,076 / 327,680 (62.6%) | 577,381 / 3,342,336 (17.3%) |
| `proposal_benchmark_base` | BASE | 23 | 205,076 / 327,680 (62.6%) | 577,381 / 3,342,336 (17.3%) |
| `proposal_replay_min` | MIN | 23 | 205,076 / 327,680 (62.6%) | 577,381 / 3,342,336 (17.3%) |
| `proposal_replay_mid` | MID | 23 | 205,076 / 327,680 (62.6%) | 577,381 / 3,342,336 (17.3%) |
| `proposal_replay_full` | FULL | 23 | 205,076 / 327,680 (62.6%) | 577,381 / 3,342,336 (17.3%) |
| `proposal_benchmark_legacy` | LEGACY比較 | 23 | 205,076 / 327,680 (62.6%) | 577,381 / 3,342,336 (17.3%) |

フラグは現時点でホスト評価の識別と二重安全ガードに限定しているため、既存ファームウェアのコードサイズは変化していない。制御アルゴリズムをXIAOの実行経路へ接続した結果ではない。

## CoreS3

既存`m5stack-cores3`環境も成功した。

* RAM: 168,156 / 327,680 (51.3%)
* Flash: 1,034,237 / 6,553,600 (15.8%)

## 実行コマンド

```powershell
platformio run -d xiao-boat-control-integration -e bno_accel100_gyro100_mag20_int_3min
platformio run -d xiao-boat-control-integration -e proposal_benchmark_base
platformio run -d xiao-boat-control-integration -e proposal_replay_min
platformio run -d xiao-boat-control-integration -e proposal_replay_mid
platformio run -d xiao-boat-control-integration -e proposal_replay_full
platformio run -d xiao-boat-control-integration -e proposal_benchmark_legacy
platformio run -d m5stack-cores3-telemetry-bridge
```
