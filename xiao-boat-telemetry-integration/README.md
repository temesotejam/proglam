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

現時点の制限は、二台接続・STOP/E-STOPの実機通過確認、全センサを同時に接続した長時間試験、航行制御はまだ未完了であることです。今回の目的はそれらを試験できる全体の形を早く揃えることです。
