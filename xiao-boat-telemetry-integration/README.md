## BOAT24 timing diagnostics

Firmware 0.3.2 adds low-overhead timing evidence for the RUN0013 record-gap investigation. Each BNO frame now retains its sensor timestamp, SH-2 callback time, and BNO event-queue push time; the frame header carries frame creation time. At the recorder, UART receipt, common log-queue insertion, and SD-task dequeue times are retained. If a BNO frame waits in the common log queue for 80 ms or more, one fixed-size TimingDiagnostic BIN record is added with all of those values and the latest SD write start/end time. The TXT summary reports timing_diagnostics and max_log_queue_wait_us. This avoids serial output and records only threshold breaches.

## &#22320;&#30913;&#27671;&#12475;&#12531;&#12469;&#20516;

SoftAP `XIAO-BOAT-TELEMETRY` (password `12345678`) &#12395;&#25509;&#32154;&#12375;&#12289;`http://192.168.4.1/sensors` &#12434;&#38283;&#12367;&#12392;&#12289;BNO08X&#12398;&#21152;&#36895;&#24230;&#12289;&#12472;&#12515;&#12452;&#12525;&#12289;&#22320;&#30913;&#27671;&#12434;50 ms&#12372;&#12392;&#12395;&#34920;&#31034;&#12375;&#12414;&#12377;&#12290;

`GET /api/sensors` &#12399;&#20197;&#19979;&#12434;JSON&#12391;&#36820;&#12375;&#12414;&#12377;&#12290;

- `ax`, `ay`, `az`: &#21152;&#36895;&#24230; [m/s2]
- `gx`, `gy`, `gz`: &#12472;&#12515;&#12452;&#12525; [rad/s]
- `mx_ut`, `my_ut`, `mz_ut`: &#26657;&#27491;&#28168;&#12415;&#22320;&#30913;&#27671; [&micro;T]
- `magnetic_valid`, `magnetic_accuracy`, `magnetic_age_ms`: 地磁気の有効性、較正精度（0〜3）、最新値の経過時間 [ms]
- `int_edges`, `task_fallbacks`, `service_calls`: D3 INT通知数、2 msフォールバック起床数、SH-2サービス回数
- `callback_events`, `accel_events`, `gyro_events`, `magnetic_events`: コールバックで復号した全イベント数と種別別件数
- `decode_errors`, `event_queue_drops`, `event_queue_used`, `event_queue_high_water`, `max_service_us`: 復号失敗、BNOイベントキューの損失・現在量・最大量、最大サービス時間 [us]

BOAT24は同一条件を60秒だけ実行する確認用ファームウェアで、BOAT23（180秒）の前に両ノードのcallback→BNOキュー→UART→ログ→SD経路を検証します。BOAT23では加速度・較正ジャイロを各100 Hz、較正地磁気を20 Hzで要求します。`event_queue_drops=0`、`decode_errors=0`、キュー最大量が96未満であることを、SD書込みエラー0・ログキュードロップ0と合わせて確認してください。画面表示とAPIはキャッシュ済み値だけを参照し、HTTP処理中にI2C読出しは行いません。 RUN0011では通信側ローカルBNOの3ストリームが連番欠落0となった一方、制御側からUART受信するBNOには欠落が残るため、2台全体の合格結果ではありません。
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


## 固定条件実験（2026-07-23）

I2C速度をRUN中に変更しない実験用ファームウェアを追加しました。制御側・通信側に同名のPlatformIO環境を書き込みます。環境一覧、書込み手順、SoftAP接続、API、結果の判定は [docs/FIXED_EXPERIMENT_FIRMWARE.md](../docs/FIXED_EXPERIMENT_FIRMWARE.md) を参照してください。
