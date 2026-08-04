# BNOマウント候補変換試験 RUN0063（2026-08-04）

## 結論

RUN0063は、BNO軸候補変換をESKF入力へ反映した状態での20秒試験として開始したが、ESKFの正常遷移を確認できず、候補変換の妥当性判定試験としては不成立。候補変換を原因と断定しない。試験後、XIAO COM4を安定版（BOAT_EXPERIMENT=23、恒等行列、raw ESKF入力）へ戻し、ビルド・書込み・Hash verifiedを確認した。COM3は一切操作していない。

## 候補変更

RUN0060～RUN0062の符号付きgyro結果から、暫定的に `bodyX=-sensorY, bodyY=sensorX, bodyZ=sensorZ`（XY 90度回転）を推定した。`app_config.h`の候補行列と、`main.cpp`のESKF predict入力だけに反映し、`kBnoMountValidated=false`は維持した。ビルドは成功したが、実機試験の通信・ESKF状態が安定しなかったため、採用しない。

## 試験条件と応答

- 対象: XIAO COM4 / CoreS3はAPIのみ（COM6シリアルは開いていない）
- 開始API: `POST /api/log/start?duration_s=20&bno_capture=0&bno_log_enqueue=0&uart_rx_diag=dispatch&eskf_reset_at_start=1`
- 開始応答: HTTP 202、`logging=true`、`eskf_reset_queued=true`
- RUN番号: RUN0063
- RUN0063.TXT: `normal_stop=1`, `records=1251`, `queue_drops=0`, `queue_high_water=28`, `sd_write_errors=0`, `control_frames=188`, `control_crc_errors=0`, `control_cobs_errors=1`, `control_length_errors=0`
- BINサイズ: 174,011 bytes

試験中の保存APIでは、Coreの受信フレームが約841～1452で停滞し、ESKFは `health_reason=alignment_incomplete`, `run_state=1`, `health=0`, `mount_valid=0` のままだった。したがって、候補行列の軸変換が正しいか、誤っているかをこのRUNから判定してはいけない。

## 復旧

候補行列とinline変換をソースから除去し、安定版の恒等行列および `shadowEskf.predict(s.rxUs,s.v,primaryImu.accel,...)` に復帰。PlatformIO buildは成功（RAM 204,308 / 327,680、Flash 576,389 / 3,342,336）。COM4へ書込み成功、MAC `34:85:18:AB:FA:90`、Hash verified。COM3は未接続・未操作。

復旧後のCore API:

- `connected=true`
- `sequence_gaps=0`, `crc_errors=0`, `length_errors=0`
- `cobs_errors=1`（XIAO再起動時から残る値。今回の候補変換の新規エラーとは断定しない）
- `shadow_only=true`, `actuator_output_enabled=false`
- ESKF `health_reason=alignment_incomplete`, `run_state=1`, `health=0`, `mount_valid=0`

ESKFは復旧後もalignment incompleteであり、正常運用合格とは扱わない。追加の実機試験や候補変換再適用は、まずこの状態の原因（リセット後のESKF初期化・アライメント遷移と通信再開）を別途切り分けてから行う。

## 保存物

`pc-tools/boat_eskf/captures/BNO_MOUNT_CANDIDATE_STATIC20_20260804/`

- `start_response.json`
- `link_before.json`, `link_13s.json`, `link_after.json`
- `eskf_before.json`, `eskf_05s.json`, `eskf_13s.json`, `eskf_after.json`
- `files_after.json`
- `RUN0063.BIN`, `RUN0063.TXT`
- `api_link_after_rollback.json`, `api_eskf_after_rollback.json`, `api_log_files_after_rollback.json`

このRUNは「候補変換の検証成功」ではなく、「候補変換試験が成立せず、安定版へ戻した」記録である。