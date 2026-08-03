# BNO08X Pitch軸切り分け RUN0061（2026-08-03）

## 条件

- XIAO COM4：`BOAT_EXPERIMENT=21`（Gyroscope/GVR 100 Hz）を維持
- CoreS3 COM6：Web APIのみ。シリアルは開いていない
- COM3：未操作
- 20秒自動取得、`bno_capture=1`、`bno_log_enqueue=1`、`uart_rx_diag=dispatch`、`eskf_reset_at_start=1`
- 操作：基準姿勢 → Pitch +90°保持 → Pitch −90°保持 → 基準姿勢

## 保存結果

- BIN：`RUN0061.BIN`、667,622 bytes、5,989 records、trailing byte 0
- TXT：`RUN0061.TXT`、`normal_stop=1`
- Gyro：2,002件、GVR：2,003件
- queue drop：0、SD write error：0、queue high-water：31
- UART sequence gap、CRC、COBS、length error：0
- GVR四元数ノルム：平均1.000003、最小0.999960、最大1.000042

## 時刻・sequence

Gyro/GVRともcallback timestampは単調増加し、report sequence不連続は0でした。callback間隔は約10 msで、RUN0059のような長時間欠測やreport sequenceリセットは再発しませんでした。SH-2 sensor timestampの非増加は32 bit wrapの影響です。

## 姿勢変化

5秒区間平均（Roll, Pitch, Yaw deg）は次のとおりです。

| 区間 | Roll | Pitch | Yaw |
|---|---:|---:|---:|
| 0–5 s 基準 | -176.61 | 0.51 | -171.50 |
| 5–10 s 回転保持 | -113.68 | 1.63 | -173.44 |
| 10–15 s 保持 | -71.33 | 1.15 | -175.37 |
| 15–20 s 復帰・遷移 | 98.71 | 1.79 | -168.76 |

Pitch軸として操作した回転は、EulerではRoll成分が約-176°から+99°へ大きく変化し、Pitch成分は約0–2°に留まりました。Roll/Pitchの表示は±180°境界をまたぐため、単純な差ではなく四元数と取付姿勢を併せて解釈します。この結果は、RUN0060と合わせて、機体の物理軸とBNO Euler軸が入れ替わっている、または取付姿勢による軸変換があることを強く示します。

## 判定

RUN0061はraw取得・通信・SD保存・report sequenceの観点で合格です。BNOリセット再発はありませんでした。軸対応は次のYaw単独試験で確認を続けます。3軸試験完了後、XIAOを`BOAT_EXPERIMENT=23`へ戻します。

## 証跡

`pc-tools/boat_eskf/captures/BNO_AXIS_PITCH_20S_20260803/`

