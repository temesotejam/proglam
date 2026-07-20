# XIAO PCA9685 servo SD controller

PCA9685を使うCH0サーボの安全確認専用プロジェクトです。VESC、PPM、航行制御は含みません。起動、Disarm、E-STOP、Web watchdogでは必ずPCA9685 OEをHighにし、CH0～CH15をFull OFFにします。

## 配線

| PCA9685 | XIAO ESP32S3 Sense | 備考 |
|---|---|---|
| VCC | 3.3 V | ロジック電源 |
| GND | GND | 外部サーボ電源GNDとも共通 |
| SDA | D1 / GPIO2 | I2C 400 kHz |
| SCL | D0 / GPIO1 | I2C 400 kHz |
| OE | D2 / GPIO3 | Active Low。10 kΩで3.3 Vへプルアップ |
| V+ | 外部安定化6 V以上 | LD-60MGのサーボ電源。XIAOへ接続しない |
| CH0 PWM | サーボ信号 | CH1～CH15は使用しない |

サーボの赤線はPCA9685 V+、黒/茶線はPCA9685 GND、信号線はCH0 PWMへ接続します。外部サーボ電源GNDとXIAO GNDは共通にします。LD-60MGの動作電圧は6 V／7.4 V／8.4 Vです。サーボをXIAOの3.3 VやUSB 5 Vから給電しません。

## 安全設定

- PCA9685 address: `0x40`、I2C: 400 kHz、PWM: 約50 Hz
- CH0のみ使用。CH1～CH15は常にFull OFF。
- Hard limit: 500～2500 µs（LD-60MG仕様値）
- Webで指定できる範囲: 500～2500 µs、中心1500 µs。500/2500 µsはLD-60MG仕様上の上限・下限。
- slew rate: 1000 µs/s（50 Hz制御では最大20 µs/更新）
- E-STOPは再起動まで解除できません。
- Arm中はWeb UIが500 msごとにheartbeatを送ります。1,000 msを超えるとOE High、全CH OFF、Disarmします。

## Web UI

SoftAP: `XIAO-PCA9685-SERVO` / password: `12345678`。接続後 `http://192.168.4.1/` を開きます。JSONは `/api/status` です。

Start logだけではArmしません。Arm後にのみCH0を1500 µsから有効化します。Stop logは先にDisarmします。Web UIは状態、OE、PWM設定、target/output、count、heartbeat、I2C/SDエラーと4 Hz履歴グラフを表示します。

## SDログと変換

`/PCALOG/RUNxxxx.BIN`と正常Stop後のTXTを作成します。BINは256 byte header、160 byte fixed record、CRC32付きです。PCA_CONFIG、SERVO_COMMAND、SERVO_OUTPUT、STATE_CHANGE、ESTOP、WATCHDOG、PCA_STATUS、LOG_ENDを含みます。

SDカードからPCへコピーしてから変換します。出力先は必須指定で、SDカード直下にはCSVを書きません。

```powershell
python tools\pca9685_bin_to_csv.py E:\PCALOG\RUN0001.BIN --out C:\Users\arika\Documents\PCA9685_results\RUN0001
```

## ビルド

```powershell
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run --target upload
```

USB serialは115200 bpsです。初回試験はPCA9685を接続してもサーボ電源とサーボ本体を外し、0x40検出、MODE1/MODE2/PRE_SCALE、OE High、全CH OFFを確認してください。サーボ電源を入れる前に、WebのDisarmed状態とE-STOP動作を確認します。
## 10秒エンドポイント往復試験

Web UIで `Start log`、`Arm` の順に操作してから `10 s endpoint sweep` を押すと、CH0を500 µsと2500 µsの間で500 msごとに切り替えます。試験時間は10秒です。試験中はファームウェア内部でheartbeatを維持し、終了時には自動でDisarmします。つまりOEはHigh、CH0からCH15はFull OFFとなります。E-STOP、Disarm、Stop log、手動パルス指令でも試験は直ちに中止されます。

この試験は急反転するため、サーボを手で拘束しないでください。外部電源の電流制限を有効にし、電源電圧低下とピーク電流を観測してください。XIAO/PCA9685は電流を測定しないため、SDログにはPWM指令と出力時刻のみが記録されます。