# XIAO ESP32S3 Sense: BNO08X + GNSS + microSD logger

## 目的
RUN0010で確認したBNO08Xの基準構成を固定したまま、GT-505GGBL5-DR-NのUART/NMEAを受信し、生NMEA・解析済みFix・受信状態を同じmicroSD BINログへ記録する最低構成です。GNSSとBNO08Xの融合、PPS、地図、DR解析、制御、D6/D7通信は実装していません。

## 配線

|機能|XIAO pin / GPIO|接続先|
|---|---|---|
|BNO SDA|D4 / GPIO5|BNO08X SDA|
|BNO SCL|D5 / GPIO6|BNO08X SCL|
|BNO INT|D3 / GPIO4|BNO08X INT（監視のみ）|
|BNO RST|D2 / GPIO3|BNO08X RST|
|GNSS UART RX|D0 / GPIO1|GT-505GGBL5-DR-N TX|
|GNSS UART TX|D1 / GPIO2|GT-505GGBL5-DR-N RX|
|SD SCK|D8 / GPIO7|Sense microSD SCK|
|SD MISO|D9 / GPIO8|Sense microSD MISO|
|SD MOSI|D10 / GPIO9|Sense microSD MOSI|
|SD CS|GPIO21|Sense microSD CS|

BNO08XとGNSSは3.3 V、GND共通です。D6/GPIO43とD7/GPIO44は将来予約で、今回使用しません。

## 固定したBNO08X基準

- I2C: 100 kHz
- アドレス: 0x4A
- 加速度・校正済みジャイロ要求間隔: 5 ms
- Game Rotation Vector要求間隔: 20 ms
- 既知の受信実測: RUN0010相当で約157/83/29 Hz（加速度/ジャイロ/Rotation Vector）

このGNSS追加ではBNO08Xの取得方式・要求周期・I2Cクロックを変更していません。

## GNSS UART

`include/app_config.h` の `kGnssBaud`、`kGnssRxPin`、`kGnssTxPin` をビルド前に変更できます。既定は115200 bpsです。GT-505GGBL5-DR-Nメーカー資料はUARTを9600～921600 bps、既定115200 bps、NMEA 0183 4.00/4.10対応としています。モジュールを設定変更済みの場合は実機設定に合わせてください。

GNSS専用HardwareSerial(1)の受信リングバッファは2048 bytesです。loopは最大512 bytesずつ取り出し、NMEA待ちのdelayやUART処理中のSD書込みは行いません。UARTドライバが内部で破棄したオーバーフローをArduino APIから直接読めないため、`uart_overflow_observable=0` として扱います。過長文・未完了文は別カウンタで検出します。

## 対応NMEA

チェックサムが正しい文を対象に、GGA、RMC、GSA、VTG、ZDAを解析します。GSVや独自DR文は解析値にせず、GNSS_RAWへそのまま保存します。

- GGA: Fix品質、衛星数、HDOP、高度、位置、UTC時刻
- RMC: Fix有効、位置、対地速度（knotsからm/sへ変換）、Course、UTC日時
- GSA: PDOP、HDOP、VDOP
- VTG: 対地速度、Course
- ZDA: UTC時刻・日付

DR状態は標準NMEAとして識別できる仕様根拠がないため、現状は有効値を生成しません。独自文はRAWで確認できます。

## 時刻と有効性

GNSS_FIXには、NMEAのUTC時刻・UTC日付、NMEA文終端をXIAOが受信した`rx_end_us`、解析完了時刻`parse_complete_us`、最終Fix時刻からの年齢を保存します。SD書込み時刻を測定時刻として使いません。各項目は`valid_flags`と個別の`*_valid`列で判定します。FixなしでもRAWと状態ログは継続します。

## SD BIN v3

v3は256 byteヘッダ、160 byte固定レコードです。v1/v2の80 byteログを変換ツールは引き続き読めます。v3で追加した種別は以下です。

- `GNSS_RAW`: `$`からチェックサムまでのNMEA原文（CR/LF除外、最大108文字）、受信終端時刻、種別、チェックサム結果、切詰めフラグ
- `GNSS_FIX`: 位置、速度[m/s]、Course[deg]、高度[m]、衛星数、HDOP/PDOP/VDOP、UTC、Fix、年齢、各有効フラグ
- `GNSS_STATUS`: 1秒あたりの受信bytes・文数・正常文数・チェックサムエラー・パースエラー・過長文・GNSSログドロップ、最終RX/Fix年齢

GNSSも既存RAMキューとSD書込みタスクを使います。キューが満杯ならUART処理を待たせず、GNSS側`log_drops`へ記録します。

## Web UI / API

SoftAP: `XIAO-BNO08X-SDLOG` / password `12345678`  
URL: `http://192.168.4.1/`  
JSON: `GET /api/latest`

Web UIは2 Hzで、GNSS baud、RX状態、bytes/s、NMEA数/s、正常文数/s、Fix、位置、高度、速度、Course、衛星数、HDOP、チェックサム/パースエラー、ログドロップ、BNOとSD状態を表示します。HTTPハンドラはI2C/UART/SD読取りを行いません。

## ビルド・書込み・CSV変換

```powershell
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run -t upload --upload-port COM5
python tools\bnolog_to_csv.py E:\BNOLOG\RUN0011.BIN
```

CSVは`RUNxxxx_gnss_raw.csv`、`RUNxxxx_gnss_fix.csv`、`RUNxxxx_gnss_status.csv`を追加出力します。Python標準`csv`を使うため、NMEA内のカンマや引用符はCSVとして安全にエスケープされます。

## 実機試験

1. GNSSを外して起動し、BNO08X、SD、Webが従来どおりで、GNSSが`NO RX`となることを確認する。
2. GNSSを接続して空が見える場所で起動し、Web/USBシリアルでbytes/sとNMEA数/sが増えることを確認する。
3. Start後30秒記録し、`*_gnss_raw.csv`のNMEA原文とチェックサム結果を確認する。
4. Fix後1分記録し、`*_gnss_fix.csv`の位置、速度、Course、衛星数、HDOPとUTC有効フラグを確認する。
5. BNO08X+GNSS+Web+SDで5分記録し、BNO停止なし、GNSS RX継続、SDエラー0、キュードロップ0を確認する。

## 未確認事項

実機GNSSの現設定、実受信ボーレート、NMEA出力種別、Fix、DR独自文、5分同時記録は未確認です。PPS、DRの高度な解析、地図、ESKF、航法・制御は今回の対象外です。

## 参考

- GT-505GGBL5-DR datasheet: https://akizukidenshi.com/goodsaffix/YIC-GT-505GGBL5-DR.pdf
- 参考実装: https://github.com/temesotejam/2026_05_20_xiao_esp32s3_sense_gnss_bno08x_web_monitor