# 固定条件実験ファームウェア

2026-07-23 作成。`RUN0001` で確認された I2C 100 kHz から 400 kHz への復帰失敗を切り分けるため、各測定は一つの固定条件だけで実行する。

制御側と通信側には、必ず同じ PlatformIO 環境名のバイナリを書き込む。測定開始時の `BenchmarkPrepare` は条件照合と安全確認だけを行い、`Wire.end()`、I2C 速度変更、INA/ToF の再初期化は行わない。BNO08X 専用バスは常に 100 kHz、制御側の ToF/INA/PCA9685 共有バスは各環境が指定する速度で起動から終了まで固定される。

## 実験と環境名

| 計画 | PlatformIO 環境 | 固定時間 | 目的 |
| --- | --- | ---: | --- |
| P0 | `p0_bringup_400k` | 1 分 | 配線、SoftAP、SD、UART、DRY_RUN、STOP/E-STOP の確認 |
| P1 | `p1_stability_400k` | 10 分 | 400 kHz 基準RUN |
| P2 | `p2_i2c_100k` | 10 分 | 100 kHz 固定RUN。測定後は電源再投入して次の400 kHz環境へ移る |
| P3 | `p3_tof_8x8_10` / `p3_tof_8x8_15` / `p3_tof_4x4_15` / `p3_tof_4x4_30` | 各5分 | ToF 解像度・周波数の比較 |
| P4 | `p4_ina_current` / `p4_ina_balanced` / `p4_ina_fast` | 各5分 | INA226 変換設定の比較 |
| P5 | `p5_uart_base` / `p5_uart_expected` / `p5_uart_double` / `p5_uart_target70` | 各5分 | 合成UART負荷 0 / 250 / 500 / 630 Hz の比較 |
| P6 | `p6_composite_10min` | 10 分 | 基準ToF・INA current・UART 250 Hz の組合せ確認 |
| P7 | `p7_endurance_60min` | 60 分 | P6 と同条件の耐久RUN |

P3以降の初期選択値は、比較を明確にするため INA current、ToF 8x8/10 Hz、共有I2C 400 kHz である。P3/P4/P5の結果から最終条件を選ぶ段階では、選定した条件だけを新しい環境として追加し、既存のRUNを上書きして比較しない。

## ビルドと書込み

例として P2（100 kHz 固定）を作る場合は、次をそれぞれ実行する。

```powershell
cd xiao-boat-control-integration
pio run -e p2_i2c_100k -t upload

cd ..\xiao-boat-telemetry-integration
pio run -e p2_i2c_100k -t upload
```

2台をUSBで同時に認識させる場合も、書込み先ポートを確認して一台ずつ実行する。片側だけ異なる環境名を使うと、通信側の開始条件照合で失敗し、測定は開始しない。

## 操作と結果

通信側XIAOのSoftAP `XIAO-BOAT-TELEMETRY`（パスワード `12345678`）に接続し、`http://192.168.4.1/` を開く。画面は50 ms（20 Hz）更新で状態とToFフレーム数の推移を表示する。開始ボタンを押すと一つの固定条件だけを測定し、通信側microSDの `/BENCH/RUNxxxx.BIN` と `/BENCH/RUNxxxx.TXT` に保存する。

`GET /api/benchmark` は測定状態、固定I2C速度、SD書込みエラー、キュードロップ、INA fresh/duplicate、ToFフレーム数、合成UART受信数、I2Cエラー、最大読出し時間をJSONで返す。TXTの `experiment=` は書き込んだ環境に対応する固定条件名である。

合格判定は `normal_stop=1`、`sd_write_errors=0`、`queue_drops=0`、`log_fault=none` を前提に、各実験計画で定めたGNSS往復差、ToF実効Hz、INA fresh率、I2C時間、UART CRC/COBSエラー、最小free heapを比較して行う。機器未接続のため、この版はビルド検証のみで実機合格とはしていない。
