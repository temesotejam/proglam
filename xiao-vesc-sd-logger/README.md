# XIAO VESC SD logger

## 目的

XIAO ESP32S3 Sense と VESC 7.00 の UART テレメトリを、安全な読取り専用で microSD に記録し、Web UI で確認する Stage 1C プロジェクトです。BNO08X、GNSS、ToF、制御、モータ指令は実装しません。

開始時は VESC のファームウェア情報を取得し、その後に **COMM_GET_VALUES (ID 4) だけを 1 Hz** で送信します。電流・回転・ブレーキ・設定変更などのコマンドは送信しません。

## 配線

| VESC | XIAO ESP32S3 Sense |
|---|---|
| UART RX | D6 / GPIO43（XIAO の TX） |
| UART TX | D7 / GPIO44（XIAO の RX） |
| GND | GND |
| 5 V | 接続しない |

UART は 115200 bps、8N1 です。VESC と XIAO はそれぞれの電源で動かし、GND だけを必ず共通にします。モータ・プロペラが不用意に動かない安全な状態で接続してください。

## Web UI

XIAO の SoftAP に接続します。

- SSID: `XIAO-VESC-LOGGER`
- パスワード: `12345678`
- URL: `http://192.168.4.1/`
- JSON API: `http://192.168.4.1/api/latest`
- Web 更新: 4 Hz（センサ取得とは独立）

画面には VESC 検出状態、FW、VALUES の年齢、ERPM、duty、モータ/入力電流、入力電圧、MOS/モータ温度、fault、UART・SD・キュー・エラー状態を表示します。HTTP ハンドラ内で UART 読取りや SD 書込みはしません。

## ログ

`/VESCLOG/RUNxxxx.BIN` は固定長 `VESC_BIN_V1`（ヘッダ 256 bytes、レコード 160 bytes、CRC32）です。すべて RAM キューを経由して SD タスクが書き込みます。

- `VESC_FW_INFO`: FW、HW 名、UUID（得られる場合）
- `VESC_VALUES`: MOS温度 [°C]、モータ温度 [°C]、モータ電流 [A]、入力電流 [A]、入力電圧 [V]、duty [-]、ERPM [electrical RPM]、Ah、Wh、タコメータ、fault
- `VESC_RAW_HEADER` / `VESC_RAW_BLOCK`: 受信フレーム原文と CRC/長さ情報
- `SYSTEM_STATUS`: UART、要求/応答数、通信/SDエラー、キュー状態
- `LOG_END`: 正常 Stop の終了記録

TXT 要約には FW/VALUES の要求・応答数、エラー、RTT、SD 書込み状態を保存します。

変換:

```powershell
python tools\vesc_bin_to_csv.py E:\VESCLOG\RUN0004.BIN
```

`RUNxxxx_vesc_fw.csv`、`RUNxxxx_vesc_values.csv`、`RUNxxxx_vesc_raw.csv`、`RUNxxxx_system_status.csv` を作成します。Stage 1A/B の旧ログも固定レコードとして読めますが、Stage 1C 以前では values CSV は空です。

## ビルド・書込み

```powershell
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run --target upload --upload-port COM5
```

USB シリアルは 115200 bps です。

## 実機試験

1. VESC 未接続で起動し、Web と SD が動作し、VESC が WAITING と出ることを確認します。
2. RX/TX/GND を接続して Start log を押します。FW が表示され、VALUES requests/responses が約 1 Hz で増えることを確認します。
3. 10 秒記録して Stop log を押し、BIN/TXT と values CSV の行数を確認します。
4. 5 分記録し、CRC、timeout、SDerr、drop が 0 を目標に確認します。

## 既知の状態

FW 7.00 の実機では FW 応答（31 bytes）、CRC、往復約 8.8 ms、SD 保存まで確認済みです。FW 応答に firmware-name 文字列が含まれないため、空欄は欠損ではなく実機応答どおりです。Stage 1C の VALUES 実機取得・5 分同時記録は、この書込み後に確認が必要です。
