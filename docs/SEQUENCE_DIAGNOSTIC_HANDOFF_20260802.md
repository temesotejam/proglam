# UART sequence / BNO個別試験 引き継ぎ（2026-08-02）

## 現在の実機状態

- CoreS3（COM6）：Core到着時刻統計、START/STOP境界ACK、sequence gap履歴、XIAO送信履歴照合を含む最新版を書込み済み。ROM no-stub書込み、全イメージ hash verified。
- XIAO（COM4）：最後に書込み済みなのは `bno_k4_magnetic_10s`（BOAT_EXPERIMENT=26）。
- UART：921600 bps、Core GPIO8=RX / GPIO9=TX、既存構造を維持。
- SD：10 MHz、既存バッファ/mutex/ログ形式を維持。
- actuator_output_enabled=false、shadow_only=true、試験はdry状態。
- 60秒静止試験・dispatch enqueue試験は未実施。ここで停止している。

## 採番と境界の確定事項

- `Header.sequence` は `uint32_t`。XIAO側では共通の `linkSeq` を全送信型で共有し、`linkSend()`内でencode/queue投入より前に採番する。
- encode失敗またはqueue満杯でも採番済み番号は消費し、XIAO `TX_HISTORY` にDropを残す。P1ゲートによる意図的抑制は採番しない。
- UART writeのpartial/zeroはTX_HISTORY flags（0x01 allocated, 0x02 enqueued, 0x04 completed, 0x08 drop, 0x10 partial, 0x20 zero, 0x40 boundary ACK）で記録する。
- START_ACKはACK自身のsequenceと、次に送る対象のfirst_sequenceを持つ。STOP_ACKはACK自身のsequenceと、最後の対象last_sequenceを持つ。ACKは同じ共通採番だが、試験対象の閉区間には含めない。
- CoreはSTART_ACK受信後のfirst_sequenceからSTOP_ACKのlast_sequenceまでを対象に、欠落をuint32_tのwrap-aware差分で計算する。
- 欠落時は `SEQUENCE_GAP` に時刻、prev/next、欠落範囲、型、RX buffer、decoder時間、RX task時間を出す。

## 実施結果

### K3 Game Rotation Vector only（RUN0027、10秒）

保存先：`pc-tools/boat_eskf/captures/BNO_K3_SEQ_LOG10_20260802/`

- HTTP start：202、`bno_capture=true`、`bno_log_enqueue=false`。自動停止、STOP_ACK、flush/close/TXT/BIN回収は成功。
- 区間：START_ACK=66452、expected first=66453、STOP_ACK=68225、expected last=68224。Core受信 first/last=66453/68224。
- Core：frames=1782、BNO kind3=500、CRC/COBS/length=0/0/0、RX full=0、queue drop/SD error=0。
- XIAO：kind3 callback=505、trial TX event/enq/done=500/500/500、drop=0、partial=0、zero=0、requested 50 Hz、effective 50.014 Hz。
- `SEQUENCE_GAP`：missing 67030 と 67031。XIAO `TX_HISTORY` は両方とも flags=0x07（allocated/enqueued/completed）で、Coreに届いていない。現時点の分類は C（TX complete → Core receive 間）。
- 判定：不合格（同一区間 gap=2）。

### ALL 5 reports（RUN0028、10秒）

保存先：`pc-tools/boat_eskf/captures/BNO_ALL_SEQ_LOG10_20260802/`

- 区間：expected first=1668、expected last=5418。Core first/last=1668/5418、STOP_ACK受信。
- Core：frames=3765、BNO kind=630/499/500/250/500、seq_gap=26、CRC/COBS/length=0/0/0、RX full=0。
- XIAO：TX event/enq/done=630/499/500(rotation)/250/500(linear)、drop/partial/zero=0。型4はrotationとlinearの共通wire typeなのでTXTYPE type=4は合計1000。
- effective Hz：kind1=63.063、kind2=50.018、kind3=50.012、kind4=25.006、kind5=50.012。要求値（K1/K2/K3/K5=50 Hz、K4=20 Hz）との差が残る。
- `SEQUENCE_GAP` は多数。代表的に1941/1942、1990/1991、1997/1998、2501/2502、2620/2621、2815/2816、3174/3175、3628–3630、4913/4914。後半の3629/3630はXIAO TX_HISTORY flags=0x07で確認でき、C候補。前半は2048件履歴リング外のため確定保留。
- 判定：不合格（gap=26、K1/K4実効Hz未達）。

### K1 Accelerometer only（RUN0029、10秒）

保存先：`pc-tools/boat_eskf/captures/BNO_K1_SEQ_LOG10_20260802/`

- 区間：expected first=1213、expected last=3042。Core first/last=1213/3042、STOP_ACK受信。
- Core：BNO kind1=631、`BNO_CORE_TS kind=1 count=631 first_rx_us=94507051 last_rx_us=104504516 delta_us=10265/15868.99/33039`。CRC/COBS/length=0/0/0、RX full=0、seq_gap=6。
- XIAO：report kind1 report_id=1、enable interval=20000 us、enable_ok=2、enable_fail=0、trial TX event/enq/done=631/631/631、drop=0。最終全体metricsのeffective_hzは63.018（callback metricsは起動後累積のため、trial event countとは別）。
- `BNO_TS ACCEL` と `BNO_VEC` は生シリアルへ保存済み。ログ要求時に `bno_log_enqueue=0` としたためCore BINへBNO payloadは入らず、試験区間のsensor timestampをBINから再計算することはできない。明日、必要ならXIAO側でtrial-baseline付きsensor timestamp統計を追加する。
- 判定：周期設定は20,000 usで確認、Core到着周期は約15.869 ms平均・10.265–33.039 ms。sequence gap=6のため合格ではない。

### K4 Magnetic only（RUN0030、10秒）

保存先：`pc-tools/boat_eskf/captures/BNO_K4_SEQ_LOG10_20260802/`

- 区間：expected first=1279、expected last=2684。Core first/last=1279/2684、STOP_ACK受信。
- Core：BNO kind4=200、`BNO_CORE_TS kind=4 count=200 first_rx_us=100445463 last_rx_us=110390478 delta_us=47075/49974.95/53863`。CRC/COBS/length=0/0/0、RX full=0、seq_gap=2。
- XIAO：report kind4 report_id=3、enable interval=50000 us、enable_ok=2、enable_fail=0、trial TX event/enq/done=200/200/200、drop=0。effective_hz=20.012。
- 欠落2223/2224はXIAO TX_HISTORY flags=0x07（type56/type16）でCore未受信。C候補。
- `BNO_TS MAG` は生シリアルへ保存済み。K1と同じくsensor timestampは起動後累積metricsであり、trial区間専用ではない。
- 判定：設定周期50,000 us、Core到着平均49.975 ms（47.075–53.863 ms）を確認。sequence gap=2のため合格ではない。

## BIN/TXT・SD

各RUNのBIN/TXTは取得済み。K3/K1/K4はBIN/TXT生成、flush、close、自動停止が完了。今回の診断は `bno_log_enqueue=0` のため、BINは制御側通常ログのみで、BNO raw payloadの復号によるsensor timestamp再計算は不可。全試験でCRC/COBS/length error=0、XIAO TX drop/partial/zero=0、Core RX full=0。

## 明日の再開手順

1. この文書、`docs/PROJECT_CONTEXT.md`、`docs/WORK_PLAN.md`、`docs/WORK_LOG.md`を読む。
2. 現在のCOM4はK4ファームウェア。Core COM6はsequence診断版。電源・配線を変えず、まずアプリ診断3行以上を確認する。
3. まずK1/K4のsensor timestampを試験区間専用で出す実装（START_ACK時にXIAO BNO timestamp metrics baselineを保存し、STOP時にBNO_TS_TRIALを出す）を追加する。BNO周期・UART・SD・ログ形式は変更しない。
4. XIAO送信履歴リングを必要なら拡張するか、欠落発生時に直ちに履歴をダンプしてA/B/C/Dを確定する。現状はK3の67030/67031、K4の2223/2224がC候補。
5. K3/ALLの合格条件（同一区間gap=0、XIAO TX complete=Core受信、型別件数/encoded bytes一致、CRC/COBS/length/unknown=0、drop/partial/zero=0、RX full=0）を満たすまでdispatchへ進まない。
6. 条件を満たした場合のみ `bno_log_enqueue=1` で10秒dispatch試験、その後に機体完全静止を確認して60秒静止試験へ進む。
7. 60秒静止ではquaternion、R/P/Y、gyro、acceleration、linear acceleration、accuracy、ESKF状態、BIN復号、TXT、SD、UARTエラーを保存する。

## 保存物一覧

- `pc-tools/boat_eskf/captures/BNO_K3_SEQ_LOG10_20260802/`（RUN0027）
- `pc-tools/boat_eskf/captures/BNO_ALL_SEQ_LOG10_20260802/`（RUN0028）
- `pc-tools/boat_eskf/captures/BNO_K1_SEQ_LOG10_20260802/`（RUN0029）
- `pc-tools/boat_eskf/captures/BNO_K4_SEQ_LOG10_20260802/`（RUN0030）
- `C:\tmp\run_bno_seq_trial.py`（既存取得器を上書きしない複製版、CAPTURE_ROOTで保存先を切替）
- Coreファームウェアソース：`m5stack-cores3-telemetry-bridge/src/main.cpp`
- XIAOファームウェアソース：`xiao-boat-control-integration/src/main.cpp`、`src/bno_reader.cpp`、`include/experiment_config.h`