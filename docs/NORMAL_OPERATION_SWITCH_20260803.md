# 通常運用周期切替・起動確認（2026-08-03）

## 実施内容

RUN0057で最終ALL設定の10秒raw BNO保存試験が合格したため、XIAO側を通常運用設定へ切り替えた。対象はボート用XIAO（COM4、MAC `34:85:18:AB:FA:90`）のみ。COM3（別用途のCoreS3）は操作していない。CoreS3 COM6への書込み・設定変更は行っていない。

- PlatformIO環境: `bno_accel100_gyro100_mag20_int_3min`
- `BOAT_EXPERIMENT=23`
- 要求周期: Accelerometer 100 Hz、Gyroscope 100 Hz、Magnetic Field 20 Hz
- Game Rotation Vector / Linear Acceleration: 無効
- アクチュエータ: dry/shadow構成を維持
- UART、ESKF、SD、mutex、ログ形式: 変更なし

## ビルド・書込み

- ビルド: 成功
- RAM: 204,308 / 327,680 bytes
- Flash: 576,389 / 3,342,336 bytes
- COM4へのesptool書込み: 成功
- 全イメージのHash of data verified: 成功
- USB mode: USB-Serial/JTAG

## 起動シリアル確認

保存先: `pc-tools/boat_eskf/captures/NORMAL_OPERATION_BOOT_CHECK_20260803/xiao_boot_serial.log`

約8秒間、COM4のアプリ診断を取得した。BNOは `ready=1`、アドレス`0x4A`で初期化成功。通常運用の有効レポートはkind 1/2/4のみで、Game Rotation Vector（kind 3）とLinear Acceleration（kind 5）は無効であることを確認した。

- kind 1: requested 100 Hz、実効約126.36 Hz（BNO受理周期は約125 Hz）
- kind 2: requested 100 Hz、実効約100.05 Hz
- kind 4: requested 20 Hz、実効約25.02 Hz（BNO受理周期は約25 Hz）
- kind 3/5: report_id=0、events=0
- BNO event queue drop: 0
- Link drop: 0
- ESKF finite/covariance: 1/1

この起動確認時はCoreS3との制御UART接続を試験していないため、XIAOの安全状態は`FAULT`（`mount_unvalidated`等）となっている。これは周期切替後のBNO初期化確認のための状態であり、通常運用試験の合否判定ではない。

## 次工程

次はCOM4（XIAO）とCOM6（ボート用CoreS3）を接続した状態で、機体を静止させ、BNOのRoll/Pitch/Yaw軸確認試験へ進む。試験開始前にCore/XIAOのMACを再照合し、COM3を使用しない。raw BNO全5種の再有効化や60秒試験は、通常運用周期の軸確認結果を確認してから行う。