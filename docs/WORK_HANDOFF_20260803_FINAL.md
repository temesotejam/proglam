# 作業終了時の正確な引継ぎ（2026-08-03）

## 実機の最終状態

- ボート用XIAO：COM4、MAC `34:85:18:AB:FA:90`
- ボート用CoreS3：COM6、MAC `30:ED:A0:D4:BF:40`
- COM3：別用途のCoreS3のため、今日も接続・書込み・操作なし
- CoreS3のシリアルポートは開いていない。USB_UART_CHIP_RESET再発防止のため、状態確認はWeb APIのみ。
- XIAOは`BOAT_EXPERIMENT=23`へ復帰済み。ビルド成功、COM4書込み成功、Hash verified。
- `shadow_only=true`、`actuator_output_enabled=false`を維持。
- 復帰後Core API：link connected、sequence gap/CRC/COBS/length=0、imu age=636 us、ESKF `run_state=2`、`health=1`、`health_reason=mount_unvalidated`、`mount_valid=0`。

## 今日の試験結果

- RUN0058：GVR静止10秒。Gyro/GVR各999件、BIN/TXT/finalize正常。
- RUN0059：姿勢軸60秒。raw/SD/UART経路は正常だったが、約1.15秒欠測とreport sequence `18→0`リセットが1回あり、部分成立。
- RUN0060：物理Roll 20秒。Gyro/GVR各2000件、report resetなし。Euler Pitchが主に変化。
- RUN0061：物理Pitch 20秒。Gyro 2002、GVR 2003件、report resetなし。Euler Rollが主に変化。
- RUN0062：物理Yaw 20秒。Gyro/GVR各2002件、report resetなし。Euler Yawが主に変化。
- RUN0060–0062はすべてnormal_stop、BIN trailing=0、queue drop=0、SD write error=0、UART CRC/COBS/length=0。

## 軸確認の暫定結論

物理Roll→Euler Pitch、物理Pitch→Euler Roll、物理Yaw→Euler Yawという対応が観測された。BNO取付姿勢またはX/Y軸変換設定が未反映の可能性が高いが、ソフトウェアの軸変換値はまだ変更していない。ESKFの`mount_unvalidated`はこの保留を表す。

## 保存場所

- 詳細報告：`docs/BNO_AXIS_TEST_RUN0059_20260803.md`
- Roll報告：`docs/BNO_AXIS_ROLL_RUN0060_20260803.md`
- Pitch報告：`docs/BNO_AXIS_PITCH_RUN0061_20260803.md`
- Yaw報告・通常周期復帰：`docs/BNO_AXIS_YAW_RUN0062_20260803.md`
- 証跡：`pc-tools/boat_eskf/captures/BNO_ATTITUDE_AXIS_60S_20260803/`、`BNO_AXIS_ROLL_20S_20260803/`、`BNO_AXIS_PITCH_20S_20260803/`、`BNO_AXIS_YAW_20S_20260803/`
- 各フォルダにBIN/TXT、開始応答、ファイル一覧、link/eskf API、復号解析JSONを保存。

## 次回開始時の手順

1. このファイル、`PROJECT_CONTEXT.md`、`WORK_PLAN.md`、`WORK_LOG.md`を読む。
2. COM3には触れない。Core COM6のシリアルも開かずAPIを使う。
3. 取付姿勢からX/Y変換行列を確定し、変更前に静止基準を保存する。
4. 軸変換を変更する場合は、ビルド・COM4書込み・通常周期静止試験を別RUNとして記録する。
5. アクチュエータ出力無効・shadow onlyを維持する。

## Git同期

- 作業ブランチ最新：`0fa6c0a`（RUN0062報告・通常周期復帰）
- main最新：`4cd52ed`（PR #13マージ済み）
- 作業ツリー：終了時点でclean

