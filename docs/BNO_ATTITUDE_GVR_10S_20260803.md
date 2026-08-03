# BNO軸確認準備・Game Rotation Vector 10秒保存（RUN0058、2026-08-03）

## 条件

- XIAO: COM4、MAC `34:85:18:AB:FA:90`
- CoreS3: COM6、MAC `30:ED:A0:D4:BF:40`
- COM3:未操作
- XIAO環境: `bno_attitude100_gyro100_3min`、`BOAT_EXPERIMENT=21`
- 有効レポート: Gyroscope 100 Hz、Game Rotation Vector 100 Hz
- Accelerometer/Magnetic/Linear Acceleration:この一時診断では無効
- API: `POST /api/log/start?duration_s=10&bno_capture=1&bno_log_enqueue=1&uart_rx_diag=dispatch&eskf_reset_at_start=1`
- actuator output:無効、shadow only維持
- Coreシリアル: USB_UART_CHIP_RESET再発防止のため開かず、APIのみ取得

## 保存・復号

- RUN0058.BIN: 332,752 bytes
- RUN0058.TXT: 257 bytes
- BIN records: 2,976
- trailing bytes: 0
- TXT records: 2,976
- queue_drops: 0
- sd_write_errors: 0
- queue high-water: 31
- P1Capture START/STOP ACK: type 23を2件受信
- UART sequence gaps: 0
- CRC/COBS/length errors: 0/0/0
- Core type counts: Gyro type 3 = 999、BnoQuaternion type 4 = 999

## Game Rotation Vector静止値

BNO payloadの順序は`qx,qy,qz,qw,roll_deg,pitch_deg,yaw_deg`。RUN0058のkind 3、999件について次の値を得た。

| 値 | 平均 | 標準偏差 | 最小 | 最大 | 最初→最後の変化 |
|---|---:|---:|---:|---:|---:|
| qx | -0.011292 | 0.000013 | -0.011353 | -0.011230 | -0.000061 |
| qy | 0.998840 | 0 | 0.998840 | 0.998840 | 0 |
| qz | -0.044754 | 0.000031 | -0.044800 | -0.044678 | 0 |
| qw | 0.013270 | 0.000070 | 0.013123 | 0.013428 | -0.000122 |
| Roll (deg) | -174.851632 | 0.003531 | -174.860550 | -174.846390 | -0.000092 |
| Pitch (deg) | 1.461164 | 0.007985 | 1.444280 | 1.479210 | -0.014287 |
| Yaw (deg) | -178.638889 | 0.001491 | -178.646088 | -178.632202 | +0.006348 |

Quaternion normは平均`0.999994`、範囲`0.999990..0.999997`。accuracyはkind 3全999件で`3`。

## timestampと連番

report sequenceはkind 2/3とも欠落・重複・逆順なし。ただしSH-2 raw sensor timestampは既知のHAL形式のため単調ではなく、kind 2で逆行354件、kind 3で逆行371件（各998区間中）を確認した。これはUART/BIN順序の欠落ではない。

## 判定

raw quaternionとRoll/Pitch/Yawの保存経路は合格。今回の機体は静止したままで、Roll/Pitch/Yaw軸の符号・方向を確認するための各軸回転はまだ実施していない。したがってRUN0058は「軸確認準備試験」であり、軸判定の合否は未確定。

次はこの一時設定のまま、actuator無効を維持して、基準姿勢→Roll軸→Pitch軸→Yaw軸の順に一軸ずつ回転させる。各姿勢を固定してAPI/BINのquaternion・RPY変化方向を記録する。終了後、通常運用BOAT_EXPERIMENT=23へ戻す。