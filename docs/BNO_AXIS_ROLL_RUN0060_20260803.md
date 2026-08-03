# BNO08X Roll軸切り分け RUN0060（2026-08-03）

## 条件

- XIAO COM4：`BOAT_EXPERIMENT=21`（Gyroscope/GVR 100 Hz）を維持
- CoreS3 COM6：Web APIのみ。シリアルは開いていない
- COM3：未操作
- 20秒自動取得、`bno_capture=1`、`bno_log_enqueue=1`、`uart_rx_diag=dispatch`、`eskf_reset_at_start=1`
- 操作：基準姿勢 → Roll +90°保持 → Roll −90°保持 → 基準姿勢

## 保存結果

- BIN：`RUN0060.BIN`、666,274 bytes、5,963 records、trailing byte 0
- TXT：`RUN0060.TXT`、`normal_stop=1`
- Gyro/GVR：各2,000件
- queue drop：0、SD write error：0、queue high-water：22
- UART sequence gap、CRC、COBS、length error：0
- control frames：5,733
- GVR四元数ノルム：平均0.999998、最小0.999956、最大1.000042

## 時刻・sequence

Gyro callback timestampは単調増加、report sequence不連続0、callback間隔は平均9.995 ms（5–20 ms範囲外0）でした。GVR callback timestampも単調増加、report sequence不連続0でした。SH-2 sensor timestampは32 bit wrapを含むため非増加が残りますが、XIAO/Coreの受信・保存時刻には欠測はありません。

## 姿勢変化

5秒区間平均（Roll, Pitch, Yaw deg）は次のとおりです。

| 区間 | Roll | Pitch | Yaw |
|---|---:|---:|---:|
| 0–5 s 基準 | -176.48 | 0.27 | -172.60 |
| 5–10 s 回転保持 | -169.78 | 63.32 | -161.47 |
| 10–15 s 保持 | -169.42 | 85.57 | -159.01 |
| 15–20 s 復帰・遷移 | -2.26 | -16.80 | -146.97 |

この操作では、EulerのPitchが約+85°まで大きく変化しました。Roll/Yawは±180°境界を通過しているため単純な平均だけでは符号を断定できません。したがって、機体上で「Roll」と呼んだ回転が、センサのEuler表示ではPitch成分として主に現れる可能性があります。これはソフトウェアの軸変換またはBNO取付姿勢確認の材料になります。

## 判定

RUN0060は、raw取得・通信・SD保存・report sequenceの観点では合格です。RUN0059で見られた約1.15秒の欠測とreport resetは再発しませんでした。物理軸の名称・符号はまだ確定せず、次は同じ手順でPitch軸、続いてYaw軸を単独試験します。

## 証跡

`pc-tools/boat_eskf/captures/BNO_AXIS_ROLL_20S_20260803/`

