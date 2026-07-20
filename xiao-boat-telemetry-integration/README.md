# 通信側 XIAO 統合ファームウェア

制御側 XIAO のUARTテレメトリをmicroSDへ保存しながら、通信側に接続するGNSSと比較用BNO08Xも同じログへ保存する最初の全体縦切りです。外部Wi-Fiルーターは不要です。

## 接続と画面

1. XIAOのSoftAP **`XIAO-BOAT-TELEMETRY`** に接続します。
2. パスワードは **`12345678`** です。
3. ブラウザで <http://192.168.4.1/> を開きます。

画面は50 ms（20 Hz）ごとにキャッシュ済みの状態を表示します。加速度Zの時系列、SD記録状態、制御側リンクの最終受信時刻、比較BNO08X、GNSSの有効性・単位・鮮度を確認できます。HTTP処理中にI2Cセンサは読まないため、画面更新は計測から独立しています。

機械読み取り用の状態は `GET /api/latest` です。主なフィールドは `sd`、`logging`、`records`、`control_age_ms`、`bno`、`accel_age_ms`、`gnss_receiving`、`gnss_fix`、`gnss_age_ms`、`lat`、`lon`、`alt_m`、`speed_mps`、`sats` です。

`POST /api/log/start?confirm=1` と `POST /api/log/stop` で記録を操作できます。開始には `confirm=1` が必須で、確認なしの開始要求は拒否します。`GET /api/manual` は、SD状態、記録状態、RUN名、SD書込みエラー、記録異常理由、GNSS往復数、制御リンク状態を返します。`POST /api/control/stop` と `POST /api/control/estop` はUARTで制御側へSTOP/E-STOPを送ります。これは初期統合用の安全停止経路であり、航行制御画面ではありません。

## 配線

| 機能 | ピン |
| --- | --- |
| GNSS (GT-505) UART | D0=RX, D1=TX, 115200 bps |
| 比較用BNO08X | D2=RST, D3=INT, D4=SDA, D5=SCL, 100 kHz |
| 制御側XIAO UART | D7=RX, D6=TX, 921600 bps |
| microSD SPI | CS=GPIO21, SCK=D8, MISO=D9, MOSI=D10 |

UARTは双方のGNDを必ず共通化し、TX/RXを交差接続します。

## ログ

Web UIの「記録を開始」を押し、確認ダイアログで了承した時だけ `/BOATLOG/RUNxxxx.BIN` を作成します。起動・再起動・書込みだけではSDへファイルを作りません。SD書込みに失敗した場合は、そのRUNを直ちに停止して画面の `log fault` に表示し、同じバッファへの再試行でエラー数だけを増やし続けません。各レコードは、既存の成功済みUART→SDロガーと同じ `GOLB` マジック、受信時刻、COBS/CRC検証済みのボートプロトコルヘッダー、payloadで構成されます。通信側が生成したGNSS/BNOレコードは通信側のboot IDで区別されます。

GNSS往復・DRY_RUN・Heartbeat・STOP/E-STOP ACKの詳細は [`docs/GNSS_ROUNDTRIP_PROTOCOL.md`](../docs/GNSS_ROUNDTRIP_PROTOCOL.md) にあります。

## Automated Benchmark

通常ログとは別に、画面の **Start automated benchmark** を一度押すと `/BENCH/RUNxxxx.BIN` と `/BENCH/RUNxxxx.TXT` を1キャンペーンにつき1組だけ作成します。ブラウザを閉じても通信側XIAO内の状態機械が継続します。開始にはDRY_RUN、SD、通信側BNO、GNSS受信、制御側Heartbeat、PCA Full OFF、VESC Duty 0、制御側のPHASE_READYが必要です。

画面でQUICK（約41分＋ウォームアップ）、STANDARD（各測定を3倍）、ENDURANCE（STANDARD＋最終複合測定3時間）、CUSTOM（全フェーズ共通秒数）を選べます。ケーブル条件として`CABLE_10CM`、`CABLE_1M_DIRECT`、`CABLE_1M_DIFFERENTIAL`、`CUSTOM`、長さ、配線、プルアップ、目標距離、メモを保存します。10 cmと約1 mの比較は、電源断後にToFケーブルだけを交換して別キャンペーンで実施します。100 kHzは共用I2Cの比較専用で、通常は400 kHzです。

APIは `GET /api/benchmark`、`POST /api/benchmark/start?confirm=1&preset=QUICK`、`POST /api/benchmark/stop` です。STOPは安全な中断、E-STOPはラッチ停止です。SD最初の書込み失敗、キュードロップ、通信断はキャンペーンを中止します。解析は `python tools/analyze_benchmark.py RUN0001.BIN --output analysis_RUN0001`、比較は `python tools/compare_benchmarks.py RUN_10CM.BIN RUN_1M.BIN --output comparison_10cm_1m` を使用します。

### 実機の最小手順

1. 2台のXIAO、共通GND、GNSS、BNO、INA、PCA、約10 cmのToF配線、microSDを接続します。サーボ電源とVESC主電源は接続しません。
2. 制御側、通信側の順に書き込み、`XIAO-BOAT-TELEMETRY`（パスワード`12345678`）へ接続して <http://192.168.4.1/> を開きます。
3. `CABLE_10CM`とQUICKを選び、Start automated benchmarkを一度だけ押します。完了後にBENCHのBIN/TXTを取り出して解析します。
4. 1 m試験では両XIAOを停止し、ToFのSDA-GND、SCL-GNDを対にした約1 mケーブルへ交換してから、`CABLE_1M_DIRECT`またはDIFFERENTIALで同じ操作を行います。

実機のPASS/WARN/FAILはまだ未取得です。今回の実装は実機結果を成功として扱いません。

現時点の制限は、二台接続・STOP/E-STOPの実機通過確認、全センサを同時に接続した長時間試験、航行制御はまだ未完了であることです。今回の目的はそれらを試験できる全体の形を早く揃えることです。
