# xiao-bno08x-vl53l5cx-sd-logger

独立したXIAO ESP32S3 Sense用プロジェクトです。既存GNSSロガーは変更・参照しません。

現在は**段階1**です。microSD、SoftAP Web UI、Start/Stop、空BIN/TXT、LOG_END、正常Stopだけを実装しています。BNO08XとVL53L5CXは意図的に無効です。

- AP: `XIAO-BNO-TOF`
- password: `12345678`
- URL: `http://192.168.4.1/`
- API: `GET /api/latest`
- 保存先: `/BNO_TOF/RUNxxxx.BIN`, `/BNO_TOF/RUNxxxx.TXT`

配線予定: BNO08XはD4/D5の100 kHz、VL53L5CXはD6/GPIO43（SDA）・D7/GPIO44（SCL）の400 kHz、microSDはCS21/SCK7/MISO8/MOSI9です。二つのI2Cバスを分離します。

段階1試験: SDを挿入し、Web Start、数秒後Stop、TXTに`normal_stop=1`があることを確認します。