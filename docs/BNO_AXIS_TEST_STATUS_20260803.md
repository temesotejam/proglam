# BNO姿勢軸確認試験 状況（2026-08-03）

## 現在の判定

RUN0058のGame Rotation Vector静止取得は完了しています。GyroscopeとGame Rotation Vectorのraw経路、BIN復号、UART、SD、finalizeは正常でした。ただし、機体を実際にRoll/Pitch/Yaw方向へ回転させる操作はまだ実施していないため、軸・符号の確認試験は未完了です。

## RUN0058の確認結果

- XIAO（COM4）には軸確認用の一時設定 `BOAT_EXPERIMENT=21`（Gyroscope 100 Hz、Game Rotation Vector 100 Hz）を書込み済み。
- CoreS3（COM6）はシリアルポートを開かず、Web APIだけを使用。
- 10秒自動試験は `RUN0058`。kind 2/3 は各999件。
- BINは2,976 records、trailing byte 0、queue drop 0、SD write error 0。
- UART sequence gap、CRC、COBS、length errorは0。
- Roll/Pitch/Yaw静止平均はそれぞれ -174.851632 / 1.461164 / -178.638889 deg。
- raw sensor timestampの逆行はkind 2で354、kind 3で371。ただしreport sequenceは欠落・重複・逆順0。

## 現在の接続状態

Coreの `/api/link` は接続中で、sequence gap/CRC/COBS/lengthは0です。RUN0058終了後のため、raw BNO kind 2/3の最新時刻は古くなっています。`/api/eskf` は `imu_stale` / `run_state=0` で、通常運用の姿勢推定結果としては扱いません。shadow_only=true、actuator_output_enabled=falseは維持されています。

## 次に行う操作

ユーザーが「静止準備完了」と伝えた後、同じ一時設定で短時間のraw取得を開始し、次の順で一軸ずつ回転・静止します。

1. 基準姿勢を保持
2. Roll軸を正方向・負方向へ順に回転して保持し、基準へ戻す
3. Pitch軸を正方向・負方向へ順に回転して保持し、基準へ戻す
4. Yaw軸を正方向・負方向へ順に回転して保持し、基準へ戻す

各軸の変化方向を記録し、試験終了後に `BOAT_EXPERIMENT=23`（通常運用周期）へ戻します。COM3は今後も接続・書込み・操作しません。Core COM6のシリアル接続も、USB_UART_CHIP_RESETを避けるため行いません。

