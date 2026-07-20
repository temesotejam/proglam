# XIAO ESP32S3 + BNO08X 単体確認

## 今回の目的

小型水上ボート統合システムの最初の段階として、制御側 **Seeed Studio XIAO ESP32S3** で BNO08X を I2C 接続し、接続可否、取得周期、基板軸・符号、安定性を確認するための最小プロジェクトです。GNSS、VL53L5CX、VESC、PCA9685、SD、Wi-Fi、ESKF、制御・サーボ処理は実装していません。

## 配線と電源

| BNO08X | XIAO ESP32S3 | 備考 |
| --- | --- | --- |
| 3.3V | 3V3 | **3.3 V 電源のみ** |
| GND | GND | 共通GND |
| SDA | D3 | I2C SDA |
| SCL | D2 | I2C SCL、400 kHz |
| INT | D1 | アクティブLowのデータ通知 |
| RST | D0 | アクティブLowのハードウェアリセット |

I2Cアドレスは 0x4A を優先し、起動時スキャンで見つからない場合は 0x4B も候補として表示・選択します。BNO08X基板をI2CモードにするためのPS0/PS1設定は、使用する基板の回路図にも従ってください。

## 使用ライブラリ

`platformio.ini` で **SparkFun BNO08x Cortex Based IMU 1.0.6** を固定しています。公開APIの `getSensorEvent()` と `getSensorEventID()` を使うため、加速度・ジャイロ・Rotation Vector を同時更新とみなさず、届いたイベント種別ごとに最新値・受信時刻・シーケンス・周期統計を更新します。

このライブラリで本実装が取得する値は次です。

- calibrated accelerometer: x, y, z と report status（精度状態）
- calibrated gyroscope: x, y, z と report status（精度状態）
- Rotation Vector: quaternion i/j/k/real（本CSVでは x/y/z/w）、report status、radian accuracy（ライブラリは取得可能だがCSVの必須列外）
- 各イベントのセンサ内部タイムスタンプ: `getTimeStamp()`（診断出力の `sensor_us`）
- BNO08Xリセット理由: `getResetReason()`

本ライブラリの公開APIからは、センサごとの「校正の進捗百分率」、I2C転送ごとの詳細なエラーコード、製品IDの整形済み文字列表現は取得していません。report status は生のBNO08X/SH-2精度状態値として記録し、実機で値の遷移を確認してください。I2C ACK不可・初期化失敗は `i2c_errors`、データ未着はストリーム別タイムアウトとして診断します。

### 単位

SparkFunライブラリはCEVA SH-2の浮動小数値を返します。本実装ではライブラリが定義する SI 単位をそのまま使用し、独自の倍率変換をしていません。

- accelerometer: m/s²
- gyroscope: rad/s
- quaternion: 無次元、順序 **x, y, z, w**（ライブラリの i, j, k, real）
- Euler: deg
- `timestamp_us`、`*_age_us`、診断の `sensor_us`: µs

`sensor_us` はライブラリの `getTimeStamp()` 値です。実機では診断の受信時刻との差・単調増加・単位を確認してください。時刻同期や絶対時刻としては扱いません。

## ビルド・書込み・モニタ

```powershell
cd C:\Users\arika\Documents\PlatformIO\proglam\xiao_esp32s3_bno08x_bringup
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run -t upload
C:\Users\arika\.platformio\penv\Scripts\platformio.exe device monitor -b 921600
```

USBシリアルは **921600 bps, 8N1** です。起動時にはファームウェア名・版、ビルド日時、ピン、I2Cクロック、スキャン結果、選択アドレス、初期化結果、リセット理由、設定レポート周期、INT方式、内部タイムスタンプの利用可否を人が読める形式で表示します。

通常時のCSVは50 Hz以下（20 ms間隔）に制限し、200 Hzのセンサイベントごとに文字列出力しません。1秒ごとの `# diagnostic:` 行は人間向けで、各ストリームのシーケンス、実測平均周波数（`hz_milli` はHz×1000）、最大受信間隔、経過時間、精度状態、センサ時刻を示します。

## CSV列

```text
timestamp_us,
accel_x_mps2,accel_y_mps2,accel_z_mps2,
gyro_x_rad_s,gyro_y_rad_s,gyro_z_rad_s,
quat_x,quat_y,quat_z,quat_w,
roll_deg,pitch_deg,yaw_deg,
quat_accuracy,
accel_age_us,gyro_age_us,rotation_age_us
```

- `timestamp_us`: XIAOでCSVを組み立てた時刻（`micros()`）
- 加速度・角速度: それぞれ最後に到着した独立イベントの値。単位は列名の通り
- `quat_*`: 最後に到着した Rotation Vector、x/y/z/w 順
- `roll_deg`, `pitch_deg`, `yaw_deg`: 同じRotation Vectorから計算したEuler角
- `quat_accuracy`: Rotation Vectorイベントのライブラリreport status（生値）
- `*_age_us`: CSV時刻から各種類の最後の受信時刻を引いた値。未受信は `4294967295`

## 座標系とEuler変換

この段階では取付補正を一切適用せず、**BNO08X基板座標のまま**出力します。基板上の +X/+Y/+Z の物理的な向きは使用しているBNO08Xモジュールのシルク・回路図で確認してください。船体座標への変換や符号反転は後段でのみ追加します。

ライブラリの quaternion は **i, j, k, real = x, y, z, w** です。`attitude_math::quaternionToEulerDegrees()` は毎回正規化し、asin引数を[-1, +1]に制限して、内部はrad、出力はdegで計算します。回転順序は intrinsic **Z-Y-X**（yaw → pitch → roll、同値な extrinsic X-Y-Z）です。pitchが±90°近傍ではEuler角固有のジンバルロックがあり、roll/yawを独立に解釈できません。

## INT、RST、異常処理

- 起動時および再初期化時、D0をLowに `kResetLowMs`（10 ms）、High後 `kResetBootWaitMs`（100 ms）待機して確実にRSTします。
- D1は`FALLING`割込みで利用します。ISRは `volatile` フラグを1にするだけで、I2C・Serial・動的確保は実行しません。イベント読取りは`loop()`だけで行います。
- 1周で読むイベント数は最大12件に制限します。INTの取りこぼし対策として、INTがLowであることも`loop()`で確認します。
- 加速度、ジャイロ、Rotation Vectorを個別に500 ms以上受信できない状態（開始後2秒以降）を検出します。3秒間隔、最大3回に制限してRST→再初期化を試みます。
- 起動時に0x4A/0x4Bが見つからない場合はエラー状態を表示して停止し、無限リセットしません。
- `micros()`の時刻差は符号なし減算で計算しており、オーバーフローを跨いでも差分判定できます。
- 通常loop内で`String`やnew/mallocを繰り返しません。RST待機以外に長い`delay()`はありません。

## 10分連続試験

1. 平らで振動の少ない場所に基板を固定して起動する。
2. シリアルログを10分以上保存する。CSVは最大50 Hz、診断は1 Hzで出る。
3. 10個の診断サマリーで、`accel`/`gyro`が概ね200 Hz、`rotation`が概ね50 Hz、各`age_us`がタイムアウト未満であることを確認する。
4. `max_gap_us`、`i2c_errors`、`reinit`、`bno_resets`を記録する。再初期化や部分停止があれば、同時刻の電源・配線・I2C波形を確認する。

## 軸・符号確認

取付補正なしで、まず基板の矢印/シルクを基準にします。

1. 静止水平で加速度の重力軸を記録する。
2. 基板を各正軸回りにゆっくり+90°回し、どのgyro軸とroll/pitch/yawが正になるかを記録する。
3. z軸周りの回転ではyaw、x軸ではroll、y軸ではpitchを確認する。
4. Rotation Vectorの精度状態が低い間はyawを確定値とみなさず、磁気環境・キャリブレーション後も再試験する。

## 問題発生時

- 電源が3.3 V、GND共通、SDA=D3/SCL=D2、INT=D1、RST=D0かを確認する。
- 起動時I2Cスキャンに0x4Aまたは0x4Bが出るか確認する。出ない場合はPS0/PS1のI2Cモード設定、プルアップ、断線を確認する。
- 初期化失敗時はINTがLow固定、RSTがHighに戻ること、400 kHzで信号品質が保てることを確認する。
- 部分停止時は診断のストリーム名、`max_gap_us`、`sensor_us`、`reset_reason`を保存する。
- 実機I2C取得、センサ時刻の実単位、軸符号、実際の精度状態の意味、10分安定性は未確認です。本プロジェクトのPCテストはI2C通信を模擬しません。

## PC単体テスト

```powershell
C:\Users\arika\.platformio\penv\Scripts\platformio.exe test -e native
```

`test/test_attitude_math/test_main.cpp` は、単位quaternion、roll/pitch/yaw各+90°、非正規化quaternionの正規化、asin近傍でNaNにならないことを検査します。実機なしではBNO08X読取りの成功を仮定しません。

## Web UI（標準アクセス方法）

ファームウェアは、Wi-Fiルーターを必要としないパスワード保護済みアクセスポイントを起動します。スマートフォンまたはPCを次へ接続し、ブラウザで開いてください。

| 項目 | 値 |
| --- | --- |
| SSID | `XIAO-BNO08X` |
| WPA2パスワード | `bno08x-test` |
| URL | `http://192.168.4.1/` |
| JSON API | `http://192.168.4.1/api/latest` |
| 画面更新 | 20 ms（50 Hz） |

端末が「インターネット接続なし」と表示しても、そのままこのWi-Fiへ接続してください。Web UIは、加速度[m/s²]、角速度[rad/s]、Quaternion[-]、roll/pitch/yaw[deg]、精度status、各ストリームのage[µs]、sequence、センサ内部時刻[µs]を20 ms間隔（50 Hz）で表示します。さらに、加速度・角速度・姿勢角をそれぞれ直近約6秒分の時系列グラフとして表示します。

`/api/latest` は、画面以外のPCツールから使えるJSON APIです。`accel`、`gyro`、`rotation`はそれぞれ独立した最後の受信イベントです。`valid=false`または`age_us`が大きい値なら、その種類はまだ未受信または停止中です。Web UIからI2C読取りはせず、通常loopで収集済みの最新値だけを返すため、Webページの更新は200 Hzの取得周期を直接変えません。

Web UIのSSID、パスワード、ポート、画面更新周期は`include/app_config.h`にまとめています。今後、このワークスペースで作成・変更するXIAO ESP32S3のPlatformIOプロジェクトにも、特別に不要と指定されない限り、SoftAP Web UIとJSON APIを標準で追加します。