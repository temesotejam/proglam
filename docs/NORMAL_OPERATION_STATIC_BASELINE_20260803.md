# 通常運用・静止基準取得（2026-08-03）

## 目的

BOAT_EXPERIMENT=23の通常運用周期へ切り替えた後、BNO軸確認試験へ進む前の静止基準とCoreS3復旧状態を確認した。

## CoreS3復旧

最初のESKFリセット試行では、CoreS3のSoftAP/APIが停止し、静止基準試験は不成立だった。COM6のシリアルポートを開くと`USB_UART_CHIP_RESET`が発生し、その後アプリが起動しない状態を再現した。

対処として、コード変更なしでCoreS3 COM6（MAC `30:ED:A0:D4:BF:40`）へbootloader、partition、boot_app0、firmwareをROM no-stub/DIOで完全書込みした。全イメージでHash verified。COM3は操作していない。

完全書込み後は、COM6を開かずAPIのみを使用した場合、ping、`/api/eskf`、`/api/link`が正常応答した。以後、CoreS3のシリアルポートを開く操作は避ける。

## 静止基準試験

SDログは開始せず、`POST /api/eskf/reset`を1回送信し、約12秒間`GET /api/eskf`を毎秒取得した。

保存先:

- `pc-tools/boat_eskf/captures/NORMAL_OPERATION_STATIC_BASELINE_20260803/api_eskf_samples_after_reset.txt`
- 初回不成立証跡: `api_eskf_samples.txt`
- Coreシリアル操作で得られた短いboot出力: `core_serial.log`

結果:

- reset response: `{"queued":true,"single_shot":true}`
- `reset_count=1`
- `shadow_only=true`
- `actuator_output_enabled=false`
- `xiao_link.connected=true`
- UART sequence gaps: 0
- CRC errors: 0（完全書込み後のAPI状態）
- COBS/length errors: 0/0（完全書込み後のAPI状態）
- quaternion: 全サンプルで `[1,0,0,0]`
- Roll/Pitch/Yaw: 全サンプルで `[0,0,0]` rad
- ESKF: `run_state=2`、`health=1`、`mount_valid=0`、`finite=1`、`covariance_valid=1`
- GNSS未受信、ToF観測は更新されているが有効な静止姿勢観測には未使用

リセット後もNED位置と速度は増加し、12秒時点でおよそ`[-65.48,-224.48,5.54] m`、`[-5.80,-19.89,0.49] m/s`となった。これは静止性能の合格を意味せず、既知のESKF観測・取付未確定問題として扱う。

## 判定

この取得は「通常運用設定での静止基準」として成立したが、BNO Roll/Pitch/Yawの軸・符号確認試験ではない。通常運用設定（Game Rotation Vector無効）ではraw quaternionが送信されず、Core APIの姿勢値もalignment/mount未確定のため軸判定に使えない。

次に進むには、次のどちらかを明示する必要がある。

1. 軸確認用にGame Rotation Vectorを一時有効化した診断ファームウェアをCOM4へ書き込む。
2. 通常運用周期のまま、加速度・ジャイロの各軸を使う6面静止／各軸回転手順を定義して実施する。

現時点では、周期・SD・UART・ESKF数式を追加変更せず、軸試験の条件確定待ちとする。