# 2026-08-03 RUN0057：最終ALL raw BNO 10秒再試験

## 条件と対象
- 対象：ボート用XIAO COM4（MAC 34:85:18:AB:FA:90）／ボート用CoreS3 COM6（MAC 30:ED:A0:D4:BF:40）。COM3は未接続・未操作。
- API：`POST /api/log/start?duration_s=10&bno_capture=1&bno_log_enqueue=1&uart_rx_diag=dispatch`
- 自動停止を待ち、手動STOPは送信していない。RUN番号はRUN0057。
- `shadow_only=true`、`actuator_output_enabled=false`を維持。

## 要求・受理・実測周期
| kind | センサ | 要求 | Feature accepted | XIAO実測Hz |
|---:|---|---:|---:|---:|
| 1 | Accelerometer | 100 Hz (10000 us) | 125 Hz (8000 us) | 約126.3 Hz |
| 2 | Gyroscope | 100 Hz (10000 us) | 100 Hz (10000 us) | 約100.02 Hz |
| 3 | Game Rotation Vector | 50 Hz (20000 us) | 50 Hz (20000 us) | 約50.02 Hz |
| 4 | Magnetic Field | 20 Hz (50000 us) | 25 Hz (40000 us) | 約25.01 Hz |
| 5 | Linear Acceleration | 50 Hz (20000 us) | 50 Hz (20000 us) | 約50.02 Hz |

Feature ResponseはXIAO生シリアルの`BNO_FEATURE phase=snapshot point=after_all`を根拠にした。要求値を書き換えず、受理値と実測値を別に保存している。

## 件数の段階一致
XIAO BNO event／TX enqueue／TX complete／Core受信／BIN保存（kind別）は次のとおりで、各段階差は0。

- kind 1: 1263 / 1263 / 1263 / 1263 / 1263
- kind 2: 999 / 999 / 999 / 999 / 999
- kind 3: 499 / 499 / 499 / 499 / 499
- kind 4: 250 / 250 / 250 / 250 / 250
- kind 5: 500 / 500 / 500 / 500 / 500

Core `TRIAL_END`は`bno_kind=1263/999/499/250/500`、XIAO `BNO_TX`も同値。Core受信の型別集計はtype 2=1263、type 3=999、type 4=999（kind 3+5）、type 20=250。

## 通信・キュー・SD・finalize
- UART sequence：`first=23758 last=28260 valid=1`、`SEQUENCE_STATE gaps=0 duplicates=0 out_of_order=0 wraps=0`。
- report_seqはCoreの`BNO_CORE_EVENT`をkind別・8-bit wrap考慮で再計算し、欠落0、重複0、逆順0。
- UART CRC/COBS/length=0/0/0、unknown=0、bno_invalid=0、RX buffer full=0。
- XIAO event/TX drop=0、partial/zero UART write=0、BNO event queue drop=0。
- SD queue drop=0、SD write error=0。`LOGGER_COUNTERS enqueue_accepted=4620 queue_dequeued=4620 written_to_bin=4620 pending_in_queue=0`。
- `LOGGER_TIMING partial_writes=0 zero_writes=0`。max SD write chunk=31526 us、max queue wait=124797 us、max flush=12394 us、max close=10399 us、max mutex wait=77 us。queue high-water=79。
- SDTRACEの512 B書込みは全件`ok=1 actual=512`。flush、close、TXT open/write/closeは成功。
- 自動停止、`TRIAL_STOP_ACK`、`LOGGER_FINAL state=FINALIZED q=0`を確認。type=23 P1Capture ACKはAPI link上でSTART/STOP各1。

## BIN/TXT復号
- `RUN0057.BIN`を保存し、既存`boat_eskf.binlog.records()`で4620件を復号。trailing byte=0。
- BIN raw BNO kind：kind1=1263、kind2=999、kind3=499、kind4=250、kind5=500。段階件数と一致。
- `RUN0057.TXT`：records=4620、queue_drops=0、sd_write_errors=0、control_frames=4512、control_crc/cobs/length=0/0/0。

## 時間・timestamp・状態
- Core到着時間（kind1 first→last）：9,994,216 us（約9.994 s）。XIAO RX時間（kind1 first→last）：9,994,931 us（約9.995 s）。
- P1 capture境界：first sequence 23758、last 28260。
- SH-2 raw sensor timestampは既知の仕様・HAL由来の重複／逆行が残る（XIAO BNO_TS_TRIAL：kind1 dup=8/reverse=589、kind2=8/435、kind3=8/260、kind4=7/131、kind5=9/255）。これはreport_seq/UART sequenceの判定とは別の値であり、ESKF dt源には直接使わない。
- 試験後APIは`shadow_only=true`、`actuator_output_enabled=false`、link sequence_gaps=0、CRC/COBS/length=0。今回の指定APIでは`eskf_reset_at_start=false`のため`reset_count=0`で、ESKFのNED/速度は試験前からの累積状態として記録した。
- XIAO/Core双方にstack overflow、Guru Meditation、再起動なし。

## 判定
RUN0057は、今回指定されたALL raw BNO 10秒試験の合格条件（段階件数一致、UART/report sequence、drop/error、queue、partial/zero、auto stop、flush/close/TXT、BIN全件復号）を満たした。RUN0056の`partial_writes=2`は正常な最終短チャンクの誤カウントだったため、Core側カウンタを実actual<nだけ数えるよう修正してからRUN0057を再試験した。

通常運用周期への切替は、別途ユーザー確認を取るまで実施していない。raw sensor timestampの重複／逆行と、resetなし試験でのESKF累積ドリフトは、次の姿勢軸・ESKF評価で扱う。

## 証跡
`pc-tools/boat_eskf/captures/BNO_ALL_RAW_ENQ_10S_RETRY_20260803/` に開始API応答、終了後API応答、Core/XIAO生シリアル、RUN0057.BIN、RUN0057.TXT、capture metadataを保存。比較用に`BNO_ALL_RAW_ENQ_10S_20260803/`（RUN0056）も保持。
