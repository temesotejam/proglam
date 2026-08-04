# 提言書向け固定長ベンチマーク設計（2026-08-04）

## 目的と境界

大会向け制御・推定処理を、現在の二台XIAO ESP32S3構成で同時実行できるかを比較するためのホスト側基盤である。ここでの計測時間はPC上の時間であり、XIAOの実行時間とは扱わない。今回の実装はSHADOW計算だけで、PCA9685、VESC、ESC、サーボへ到達する経路を持たない。

`BOAT_EXPERIMENT=23`は変更せず、ファームウェア側には次のオプトインフラグを追加した。

| フラグ | 既定値 | 専用環境 | 意味 |
| --- | ---: | --- | --- |
| `BENCHMARK_ENABLE` | 0 | `proposal_benchmark_*` | 固定長評価ビルド |
| `REPLAY_ENABLE` | 0 | `proposal_replay_*` | リプレイ評価ビルド |
| `SHADOW_CONTROL_ENABLE` | 0 | 全proposal環境 | SHADOW経路の識別 |
| `ACTUATOR_OUTPUT_ENABLE` | 0 | 全proposal環境 | 0以外をコンパイル時拒否 |
| `PROPOSAL_PROFILE` | 0 | 0～4 | BASE/MIN/MID/FULL/LEGACY識別 |

`BENCHMARK_ENABLE`または`REPLAY_ENABLE`と`ACTUATOR_OUTPUT_ENABLE=1`の組合せは`static_assert`で拒否する。既存23環境では全フラグが未定義のため、既存挙動は変わらない。

## 固定長ベンチマーク

`pc-tools/boat_eskf/boat_eskf/proposal_benchmark.py`の`deterministic_sample(index)`が同じ入力を生成し、各モードを10,000回実行する。対象処理は次のとおり。

* 最低構成：GNSS変換、ウェイポイント、LOS、発進Yaw保持、COG判定、後部Yaw、Roll PD、ToF 64ゾーン品質・傾き補正・LPF、高さP、翼角合成・飽和、Safety状態機械、SHADOW出力
* 追加構成：Leaky ILOS、Gyro yaw-rate内側制御、5状態水平外乱EKF相当、2状態高さKF相当
* `LEGACY`：既存PC側15状態`ShadowEskf`を比較対象として呼び出すだけで、新制御経路には接続しない

各処理は呼出回数、合計、平均、最小、最大、p50/p95/p99、暫定deadline、NaN/Inf、入力異常、出力飽和、`tracemalloc`上の最大増分を記録する。deadline値は比較用の暫定周期であり、XIAOの正式仕様ではない。

## 実行方法

```powershell
python pc-tools/boat_eskf/tools/run_proposal_evaluation.py `
  --iterations 10000 `
  --out-dir pc-tools/boat_eskf/results/proposal_20260804
```

出力は`benchmark.json`、`replay.json`、`summary.json`である。実ログが不足しているため、今回の入力は模擬入力として明示し、実機ログと混同しない。

## 構成比較

| モード | 有効化範囲 | 用途 |
| --- | --- | --- |
| BASE | 現行のGNSS変換、安全、SHADOW出力 | 現行安定構成の基準 |
| MIN | 大会最低構成一式 | 最低限の同時実行量 |
| MID | MIN + ILOS + yaw-rate | 追加制御の比較 |
| FULL | MID + 水平EKF + 高さKF | 研究候補の上限比較 |
| LEGACY | 15状態ESKF比較呼出 | 既存負荷の参考値のみ |

FULLがホスト上で完走しても本番採用とは判定しない。XIAO上の60秒・10分SHADOW測定が必要である。
