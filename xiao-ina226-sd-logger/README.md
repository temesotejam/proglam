# XIAO ESP32S3 Sense + INA226 SD logger

## 目的

Strawberry Linux INA226 20 A module (2031、シャント抵抗 2 mΩ) を XIAO ESP32S3 Sense で 50 Hz 読み取りし、Web UI と microSD の同一ログで確認する単体試験です。BNO08X、GNSS、ToF、VESC、モーター制御、センサ融合は含みません。

## 配線

| INA226 module | XIAO ESP32S3 Sense | 備考 |
|---|---|---|
| VCC | 3.3 V | I2C プルアップも 3.3 V 側 |
| GND | GND | 被測定系の GND と共通 |
| SDA | D1 / GPIO2 | 4.7 kΩ pull-up to 3.3 V |
| SCL | D0 / GPIO1 | 4.7 kΩ pull-up to 3.3 V |
| ALERT | 未接続 | 今回はポーリング |
| A1, A0 | GND, GND | I2C address 0x44 |

microSD は CS=GPIO21、SCK=D8/GPIO7、MISO=D9/GPIO8、MOSI=D10/GPIO9 です。

電流を測る配線は、電源の正側 → `ISENSE+ / V+` → `ISENSE-` → 負荷の正側、負荷の負側 → 電源 GND です。INA226 の `V-` は被測定系 GND です。CoreS3 の 5 V を XIAO の電源へ直結しません。

## 設定と単位

- I2C: 100 kHz、INA226 address `0x44`
- 取得: 50 Hz（20 ms）。INA226 設定 `0x08DF`: average 16, bus/shunt conversion 588 µs, continuous shunt+bus。
- シャント: 0.002 Ω
- Calibration: `0x0800`。CURRENT_LSB=0.00125 A/bit、POWER_LSB=0.03125 W/bit。
- シャント電圧 = signed raw × 2.5 µV、バス電圧 = unsigned raw × 1.25 mV。
- 電流は `current_shunt_a = shunt_v / 0.002` と、校正レジスタ由来の `current_register_a` の両方を保存します。電力もレジスタ値と `bus_v × current_shunt_a` の両方を保存します。

外部 INA226 ライブラリは使用せず、Arduino framework の `Wire` で INA226 レジスタを直接読み書きしています。起動時に I2C スキャン、manufacturer ID `0x5449` と die ID `0x2260` を確認し、設定値を読み戻します。

## Web UI

XIAO は SoftAP を開始します。

- SSID: `XIAO-INA226-LOGGER`
- password: `12345678`（試験用。完成機では変更すること）
- URL: `http://192.168.4.1/`
- JSON: `http://192.168.4.1/api/latest`
- 更新: 4 Hz。ブラウザ側のバス電圧履歴グラフ、Start/Stop log ボタンを表示します。

HTTP ハンドラ内では I2C 読み取りを行いません。収集は loop 側で独立して実行されます。JSON の `valid`、`fresh`、`age_ms`、`i2c_errors`、`reinitializations`、`sd_state`、`log_drops` で状態を判断してください。

## ビルド・書込み

```powershell
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run --target upload
```

USB serial は 115200 bps です。起動時に I2C scan、INA226 ID/config/calibration、SoftAP IP を表示します。INA226 が未接続でも Web と SD は起動し、`NOT DETECTED` を表示します。

## SDログ

`/INALOG/RUNxxxx.BIN` と、正常 Stop 後の `/INALOG/RUNxxxx.TXT` を作成します。BIN はヘッダ256 byte、固定長160 byte record、各 record CRC32 です。レコード型は `INA_CONFIG`、`INA_MEASUREMENT`、`INA_STATUS`、`LOG_END`。測定は受信時刻 `rx_timestamp_us` を使い、SD 書込み時刻を測定時刻にしません。

PC へ BIN をコピーしてから、SDカードのルートではない出力フォルダを必ず指定して変換します。

```powershell
python tools\ina226_bin_to_csv.py E:\INALOG\RUN0001.BIN --out C:\Users\arika\Documents\INA226_results\RUN0001
```

出力は `RUN0001_ina_measurement.csv`、`RUN0001_ina_config.csv`、`RUN0001_ina_status.csv` です。CSV は生の各レジスタ値、換算値、測定時刻、I2C時間、エラー・キュードロップ等を含みます。

## 最小実機試験

1. INA226 を外して起動し、I2C scan に 0x44 が出ず、Web が `NOT DETECTED` のままでも SD/Web が停止しないことを確認します。
2. 配線後に起動し、0x44、manufacturer `0x5449`、die `0x2260`、config `0x08DF`、calibration `0x0800` を確認します。
3. 負荷を接続して Start log、30秒後に Stop log。BIN/TXT と CSV を確認します。
4. 5分記録では `log_drops=0`、`sd_errors=0`、`fresh=true` を目標にします。

実機の電圧・電流精度、50 Hz の実受信周期、SDとの同時5分動作は、ファームウェアのビルドだけでは未確認です。