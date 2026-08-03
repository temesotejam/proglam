# CoreS3 UART受信タスク分離・診断試験報告

実施日: 2026-08-02（JST）  
対象: `m5stack-cores3-telemetry-bridge`（CoreS3、COM6）、`xiao-boat-control-integration`（制御側XIAO、COM4）

## 1. 目的と結論

RUN0011で、XIAOから送出されたフレームがCoreS3の試験区間でほとんど復号されず、RX buffer 16 KiB張り付きが観測された。RUN0011の原因切り分けとして、SD、ログ形式、SDクロック、mutex、BNO設定を変えず、Core UART受信を専用タスクへ分離した。

A/B/Cの10秒診断はすべて、診断目的に対して合格した。

| 試験 | モード | 目的 | 判定 |
|---|---|---|---|
| RUN0012 | `raw` | UARTバイト取り込みだけ。decoder/dispatchなし | 合格 |
| RUN0013 | `decode_count` | COBS/CRC/length復号と種別集計だけ。cache/dispatch/queueなし | 合格 |
| RUN0014 | `dispatch_no_bno_enqueue` | 通常dispatch。ただしBNOをSD queueへ投入しない | 合格 |
| RUN0015 | `dispatch_no_bno_enqueue`（現行診断版） | `SDTASK max_process_us`を含む現行版の再確認 | 合格 |

A/B/CでUART CRC/COBS/length error、RX buffer full、UART queue drop、SD write errorは試験区間0で、API/Web、自動停止、flush、close、TXT生成、BIN復号も成立した。したがって、今回の専用RXタスク化で「受信取り込みがloopTaskを占有する」経路は切り分けられた。

ただし、BNO kind 3（Game Rotation Vector）とkind 5（Linear Acceleration）は全試験で0件だった。kind 3/5の設定・送信・受信試験は未成立であり、60秒静止本試験およびBNOをqueueへ投入する60秒試験へはまだ進まない。

## 2. RUN0011の帯域値訂正

RUN0011のXIAO P1区間は、COBS符号化後の送信バイト数が`263,584 byte`、実測時間が`10.002 s`である。

- encoded byte rate: `263,584 / 10.002 = 26,353 B/s`
- 8N1 physical occupancy: `26,353 × 10 / 921,600 × 100 = 28.6 %`

従来の2.9%は、8N1の10 bit/byteを掛けていない誤りである。BNOだけでなく、ToF/VESC/GNSS/ESKF/heartbeat等を含むXIAO全体のUART占有率である。

## 3. 変更前のCore構造

コード確認時点の変更前構造は次のとおりだった。

- Arduino frameworkの`loopTask`（Core 1、framework標準のpriority 1）に、`M5.update()`、WebServer、GNSS、制御UARTの`serviceControl()`、送信、診断、描画を集約。
- `serviceControl()`は`while(controlUart.available())`で受信バイトを無制限に読み、同じloopTask内でCOBS/CRC/length復号、cache、TimeSync、BNO判定、queue投入まで実行。
- `SdWriter`はCore 1、priority 1、stack 12 KiB。SDアクセスはこのwriterへ集約する既存設計を維持。
- Core制御UARTは`HardwareSerial(1)`、RX GPIO8、TX GPIO9、921600 bps、Arduino RX buffer 16 KiB。
- XIAO側のLinkTxは専用タスク（Core 0、priority/stackはXIAO側設定）、VESC UARTは別経路。

この構造ではUARTが連続非空になると、Web処理、自動停止判定、診断出力へ戻る公平性が保証されなかった。

## 4. 実装したCore変更

`m5stack-cores3-telemetry-bridge/src/main.cpp`のみ、UART取り込み公平化と診断を追加した。

- `controlRxTask`をCore 1、priority 2、stack 8192 byteで生成。
- 1回のUART読み出しは最大512 byte、`controlUart.setTimeout(2)`、1回の処理後に1 tick譲渡。
- UART readerはこのタスクだけ。decoderもこのタスクだけ。loopTaskから`serviceControl()`を呼ばない。
- RX buffer 16 KiB、UART 921600、UARTピン、SD、mutex、ログ形式、BNO設定は変更なし。
- RXタスク内ではSD、JSON、USB処理を行わない。
- 既存decoderのCOBS/CRC/length処理、種類判定、TimeSync処理は維持。
- `SdWriter`の実処理時間を`SDTASK max_process_us`として毎秒診断へ追加。フレーム平均は整数丸めで0にならないよう`service_frames_avg_x1000`も追加。

### 診断モード

`POST /api/log/start`の`uart_rx_diag`で次を選択できる。

- `raw`: バイト数・0x00 delimiterのみ。decoderを呼ばない。
- `decode_count`: decoderと種別/BNO kind集計のみ。cache、TimeSync、queue投入なし。
- `dispatch_no_bno_enqueue`: cache/TimeSyncと非BNO queue投入。BNOはqueue投入しない。
- `dispatch`: 通常dispatch。`bno_log_enqueue`がtrueならBNOもqueue投入。

毎秒/start/endに、UART B/s、総byte、read calls、zero reads、read最大、delimiter、decoded frames、BNO kind、decoder error、RX current/max/full、reader最大処理時間、連続処理時間、stack HWM、loop最大、queue/drop、heapを保存する。Arduino HardwareSerialのoverflowカウンタはAPIにないため`overflow=framework_unavailable`と明記する。

## 5. XIAO側カウンタ

`xiao-boat-control-integration/src/main.cpp`に、型ごとの`requested/enqueued/completed/encodedBytes`を追加した。P1開始でリセットし、P1停止で`P1_CAPTURE_STATS_STOP`と`TXTYPE`を出力する。LinkTxの実際の書込みは変更していない。

BNO kind対応はコード上、kind 1=加速度、2=ジャイロ、3=Game Rotation Vector、4=磁気、5=Linear Accelerationである。

## 6. 試験結果

### A: RUN0012 / raw

開始応答は`HTTP 202`、`uart_rx_diag=raw`。Core終端:

```text
TRIAL_END mode=raw rx_bytes=258560 service_bytes=258560 frames=0 bno_valid=0 seq_gap=0 crc/cobs/len=0/0/0 unknown=0 bno_invalid=0 rxbuf_full=0 service_calls=9975 read_calls=9975 zero_reads=7289 delimiters=2713 bno_kind=0/0/0/0/0 bno_log_enqueue=0
```

- RX task最大処理: 1961 us、最大read: 272 byte、RX max: 272 byte、full: 0、stack HWM最小: 4920 byte。
- queue drop: 0、SD error: 0、SDTRACE sampled writeは全て`req=512 actual=512 ok=1`。
- `RUN0012.BIN`: 61,344 byte、457 records、末尾0 byte、queue timestamp span 10.001559 s。
- `RUN0012.TXT`: records=457、queue_drops=0、sd_write_errors=0。
- XIAO P1型別総encoded bytes: 257,580 byte。kind 1/2/4は631/500/250件、kind 3/5は0件。型別の詳細は`xiao_serial_raw.log`に保存。

rawはdecoderを実行しないためframes=0が期待値であり、バイト取り込みの合格を示す。

### B: RUN0013 / decode_count

開始応答は`HTTP 202`、`uart_rx_diag=decode_count`。Core終端:

```text
TRIAL_END mode=decode_count rx_bytes=252732 service_bytes=252732 frames=2633 bno_valid=1380 seq_gap=6 crc/cobs/len=0/0/0 unknown=0 bno_invalid=0 rxbuf_full=0 service_calls=9317 read_calls=9317 zero_reads=6741 delimiters=2633 bno_kind=630/500/0/250/0 bno_log_enqueue=0
```

- RX task最大処理: 2068 us、最大read/RX max: 256 byte、full: 0、stack HWM最小: 4920 byte。
- decoder errorはCRC/COBS/length=0/0/0。BNO kind 1/2/3/4/5=630/500/0/250/0。
- `RUN0013.BIN`: 59,086 byte、442 records、末尾0 byte、queue span 10.002775 s。
- TXT records=442、queue_drops=0、sd_write_errors=0。SDTRACE sampled write、flush、close、summary open/write/closeは成功。
- XIAO P1 total encoded bytesは251,920 byte。Core側の試験境界に開始前後フレームが含まれるため、Core trial byte/frameとの差分は境界差として扱う。

### C: RUN0014 / dispatch_no_bno_enqueue

Core終端:

```text
TRIAL_END mode=dispatch_no_bno_enqueue rx_bytes=252592 service_bytes=252592 frames=2631 bno_valid=1381 seq_gap=2 crc/cobs/len=0/0/0 unknown=0 bno_invalid=0 rxbuf_full=0 service_calls=9412 read_calls=9412 zero_reads=6859 delimiters=2631 bno_kind=631/500/0/250/0 bno_log_enqueue=0
```

- RX task最大処理: 1808 us、最大read/RX max: 288 byte、full: 0、stack HWM最小: 4248 byte。
- `RUN0014.BIN`: 205,726 byte、1,712 records、末尾0 byte、queue span 10.001497 s。
- TXT records=1712、queue_drops=0、sd_write_errors=0、flush/close/TXT生成成功。
- SDTRACE sampled writeは全て512/512、CMD/SD error行なし。
- Core TRIALTYPEで非BNO受信型を確認: ToF 48、VESC 430、TimeSyncReply 10、GNSS process 99、EstimatedState 99、PrimaryImuSnapshot 196、Heartbeat 100、EskfState 196、EskfInnovation 22、EskfHealth 50。BNOはAccel 631、Gyro 500、Magnetic 250。Game Rotation Vector/Linear Accelerationは0。

### C再確認: RUN0015 / 現行診断版

追加した`SDTASK`診断を実機で確認するため、同じ条件をもう1回自動停止まで実施した。開始応答は次のHTTP 202である。

```json
{"logging":true,"duration_s":10,"bno_capture":true,"bno_log_enqueue":false,"uart_rx_diag":"dispatch_no_bno_enqueue"}
```

Core終端:

```text
TRIAL_END mode=dispatch_no_bno_enqueue rx_bytes=252896 service_bytes=252896 frames=2633 bno_valid=1380 seq_gap=4 crc/cobs/len=0/0/0 unknown=0 bno_invalid=0 rxbuf_full=0 service_calls=9455 read_calls=9455 zero_reads=6842 delimiters=2633 bno_kind=630/500/0/250/0 bno_log_enqueue=0
```

- RX task最大処理: 1972 us、最大read/RX max: 276 byte、full: 0、stack HWM最小: 4172 byte。
- `SDTASK max_process_us=106813`を自動停止後に取得。これはSD mutex待ちとwriter処理を含むタスク反復の最大値。
- `RXDIAG`最終値: loop_period_max_us=291712、loop_exec_max_us=853095、queue high water=21、drops=0。
- `RUN0015.BIN`: 205,472 byte、1,711 records、末尾0 byte、queue span 10.000366 s。
- `RUN0015.TXT`: records=1711、queue_drops=0、sd_write_errors=0、control CRC/COBS/length=0/0/0。
- SDTRACE write sampled 28行は全て512/512成功、write error/CMD error=0。flush、close、summary open/write/closeはok=1。
- XIAO `BNO_TX`最終値はkind 1/2/3/4/5=630/500/0/250/0、BNO drop=0、partial=0、zero=0。XIAOの全生シリアルは保存済み。

## 7. XIAO型別の送信帯域（RUN0012 P1、10.002秒基準）

8N1占有率は`B/s × 10 / 921600 × 100`で計算した。BNOを含む全型の合計は257,580 byte/10秒、約25,758 B/s、約27.95%である。

| 型 | 件数 | 実効Hz | encoded bytes | B/s | 8N1占有率 |
|---|---:|---:|---:|---:|---:|
| BnoAccel | 631 | 63.1 | 53,004 | 5,300.4 | 5.751% |
| BnoGyro | 500 | 50.0 | 42,000 | 4,200.0 | 4.557% |
| BnoMagnetic | 250 | 25.0 | 21,000 | 2,100.0 | 2.279% |
| TofFrame | 48 | 4.8 | 10,752 | 1,075.2 | 1.167% |
| VescStatus | 427 | 42.7 | 37,576 | 3,757.6 | 4.077% |
| TimeSyncReply | 10 | 1.0 | 560 | 56.0 | 0.061% |
| GnssProcessResult | 100 | 10.0 | 9,600 | 960.0 | 1.042% |
| EstimatedState | 99 | 9.9 | 14,652 | 1,465.2 | 1.590% |
| PrimaryImuSnapshot | 195 | 19.5 | 17,940 | 1,794.0 | 1.947% |
| Heartbeat | 100 | 10.0 | 4,000 | 400.0 | 0.434% |
| EskfState | 195 | 19.5 | 35,696 | 3,569.6 | 3.873% |
| EskfInnovation | 100 | 10.0 | 7,200 | 720.0 | 0.781% |
| EskfHealth | 50 | 5.0 | 3,600 | 360.0 | 0.391% |

非BNO合計は141,576 byte/10秒、14,157.6 B/s、約15.36%である。RUN0013/RUN0014/RUN0015は開始・停止境界を含むため、XIAO側カウンタとCore trialの差分が数フレーム発生する。

## 8. ビルド・書込み・起動確認

- CoreS3ビルド成功: RAM 150,500 / 327,680 byte（45.9%）、Flash 1,023,569 / 6,553,600 byte。
- COM6へROM no-stub書込み。bootloader、partitions、boot_app0、firmwareの全てで`Hash of data verified`。
- 書込み後COM6で8秒生シリアルを保存。診断3行以上を確認し、`service_frames_avg_x1000=130`、`RXTASK`、`SDTASK max_process_us=0`（非ログ時）を確認。XIAO UART受信、Web/loopも継続。

## 9. 保存済み証跡

- A: `pc-tools/boat_eskf/captures/UART_RX_RAW_LOG10_20260802/`
- B: `pc-tools/boat_eskf/captures/UART_RX_DECODE_COUNT_LOG10_20260802/`
- C: `pc-tools/boat_eskf/captures/UART_RX_DISPATCH_NO_BNO_ENQ_LOG10_20260802/`
- C現行版: `pc-tools/boat_eskf/captures/UART_RX_DISPATCH_CURRENT_LOG10_20260802/`
- 書込み後起動確認: `pc-tools/boat_eskf/captures/CORE_RXTASK_UPLOAD_VERIFY_20260802/`
- 以前のRUN0011補正報告: `docs/BNO_UART_DECOUPLED_ENQ0_RUN0011_20260802.md`
- 本報告: `docs/UART_RX_TASK_DIAGNOSTIC_REPORT_20260802.md`

各captureディレクトリには、Core/XIAO生シリアル、開始応答、Web応答、BIN/TXT、診断JSON、ping結果を保存している。`SDTRACE`全行はCore生シリアル内に含まれる。

## 10. 次の段階

1. kind 3/5が送信されない原因をXIAO側のBNO設定・イベント処理・`bnoEvent()`分岐で確認する。
2. kind 3/5を個別に有効化し、Coreの`TRIALTYPE`とXIAOの`BNO_TX`で送信数・受信数を突き合わせる。
3. その後にのみ、通常dispatchかつBNO queue投入ありの10秒試験を行う。
4. 10秒が完全に合格した後、機体静止条件を明示確認して60秒静止ログへ進む。

今回の結果だけで、BNO kind 3/5や60秒静止性能が合格したとは扱わない。
## 11. 型別encoded byte診断版の実機反映と再取得（追記）

前版の実機試験後、CoreS3 COM6が再列挙されない時間があったため、COM6復帰を待って型別encoded byte診断版をROM no-stub方式で再書込みした。bootloader、partitions、boot_app0、firmwareの全hash verifiedを確認し、書込み後に8秒の生シリアルで診断3行以上を保存した。

- 最新ビルド: RAM 154,596 / 327,680 byte、Flash 1,023,745 / 6,553,600 byte。
- 書込み後起動確認: `CORE_RXTASK_TYPEBYTES_UPLOAD_VERIFY_20260802/`。`RXTASK`、`RXDETAIL`、`SDTASK`を確認。
- B開始APIが一度タイムアウトした試行は不成立として別captureに保存し、RUN番号・合格数へ含めない。

### 最新B: RUN0016 / decode_count

Core終端は次のとおり。

```text
TRIAL_END mode=decode_count rx_bytes=253572 service_bytes=253572 frames=2640 bno_valid=1380 seq_gap=12 crc/cobs/len=0/0/0 unknown=0 bno_invalid=0 rxbuf_full=0 service_calls=9298 read_calls=9298 zero_reads=6907 delimiters=2640 bno_kind=630/500/0/250/0 bno_log_enqueue=0
```

`TRIALTYPE`のCore受信型別値（COBS delimiter込み）は次のとおり。

| 型 | frames | Core encoded bytes |
|---|---:|---:|
| BnoAccel | 630 | 52,920 |
| BnoGyro | 500 | 42,000 |
| TofFrame | 48 | 10,752 |
| VescStatus | 433 | 38,104 |
| TimeSyncReply | 11 | 616 |
| GnssProcessResult | 100 | 9,600 |
| BnoMagnetic | 250 | 21,000 |
| EstimatedState | 100 | 14,800 |
| PrimaryImuSnapshot | 197 | 18,124 |
| Heartbeat | 101 | 4,040 |
| EskfState | 198 | 36,432 |
| EskfInnovation | 22 | 1,584 |
| EskfHealth | 50 | 3,600 |

型別byte合計は253,572 byteでCore trial rx_bytesと一致した。XIAO側BNO_TXはkind 1/2/3/4/5=630/500/0/250/0、BNO drop=0、partial=0、zero=0。

- RX task最大処理: 2383 us、read最大/RX max: 284 byte、RX full=0、stack HWM最小=4920 byte。
- `SDTASK max_process_us=128970`。
- 最終RXDIAG: loop_period_max_us=192252、loop_exec_max_us=147917、queue high water=10、drops=0。
- trial CRC/COBS/length=0/0/0、unknown=0。TXTの累積`control_length_errors=1`は試験開始前の累積値であり、trial増分は0。
- BIN `RUN0016.BIN`: 62,589 byte、465 records、末尾0、queue span 10.000673 s。TXT、flush、close、summary open/write/close成功。SDTRACE sampled write 10行は全て512/512、CMD/SD write errorなし。

### 最新C: RUN0017 / dispatch_no_bno_enqueue

開始応答はHTTP 202:

```json
{"logging":true,"duration_s":10,"bno_capture":true,"bno_log_enqueue":false,"uart_rx_diag":"dispatch_no_bno_enqueue"}
```

Core終端:

```text
TRIAL_END mode=dispatch_no_bno_enqueue rx_bytes=253000 service_bytes=253000 frames=2637 bno_valid=1382 seq_gap=6 crc/cobs/len=0/0/0 unknown=0 bno_invalid=0 rxbuf_full=0 service_calls=9334 read_calls=9334 zero_reads=6938 delimiters=2637 bno_kind=632/500/0/250/0 bno_log_enqueue=0
```

`TRIALTYPE`型別値:

| 型 | frames | Core encoded bytes |
|---|---:|---:|
| BnoAccel | 632 | 53,088 |
| BnoGyro | 500 | 42,000 |
| TofFrame | 47 | 10,528 |
| VescStatus | 430 | 37,840 |
| TimeSyncReply | 11 | 616 |
| GnssProcessResult | 99 | 9,504 |
| BnoMagnetic | 250 | 21,000 |
| EstimatedState | 99 | 14,652 |
| PrimaryImuSnapshot | 197 | 18,124 |
| Heartbeat | 100 | 4,000 |
| EskfState | 197 | 36,248 |
| EskfInnovation | 25 | 1,800 |
| EskfHealth | 51 | 3,672 |

この型別値の合計は253,072 byteで、TRIAL_END直後にRX taskが処理した境界フレーム1件（72 byte）が`TRIALTYPE`出力へ入ったため、TRIAL_ENDの253,000 byteより72 byte大きい。これはSDやUART欠落ではなく、終了診断の非原子的な出力境界である。Core trialの判定値はTRIAL_ENDを正本とする。

- XIAO最終BNO_TX: kind 1/2/3/4/5=632/500/0/250/0、BNO drop=0、partial=0、zero=0。
- RX task最大処理: 1721 us、read最大/RX max: 268 byte、RX full=0、stack HWM最小=4248 byte。
- `SDTASK max_process_us=76144`。
- 最終RXDIAG: loop_period_max_us=194999、loop_exec_max_us=151526、queue high water=16、drops=0。
- trial CRC/COBS/length=0/0/0、unknown=0。TXTの累積`control_cobs_errors=1`は試験区間外の累積値で、trial増分は0。
- BIN `RUN0017.BIN`: 208,584 byte、1,734 records、末尾0、queue span 10.000860 s。TXT records=1734、queue_drops=0、sd_write_errors=0。SDTRACE sampled write 28行は全て512/512、flush、close、summary open/write/close成功、CMD/SD errorなし。

### 最新版の判定

RUN0016はtrial内のdecoder error 0、型別byte合計一致、queue/SD/finalize/BIN条件を満たした。TXT累積エラーは試験前からの値である。RUN0017はtrial内のdecoder error 0、通常dispatch継続、ESKF/Web/API継続、queue/SD/finalize/BIN条件を満たした。両試験ともkind 3/5は0件であり、BNO追加レポートの問題は未解決のまま別切り分け対象である。

最新capture:

- B: `pc-tools/boat_eskf/captures/UART_RX_DECODE_COUNT_CURRENT2_LOG10_20260802/`
- C: `pc-tools/boat_eskf/captures/UART_RX_DISPATCH_CURRENT3_LOG10_20260802/`
- 開始APIタイムアウトで不成立だった試行: `UART_RX_DECODE_COUNT_CURRENT_LOG10_20260802/`、`UART_RX_DISPATCH_CURRENT2_LOG10_20260802/`
- 最新書込み後起動: `CORE_RXTASK_TYPEBYTES_UPLOAD_VERIFY_20260802/`
## 12. 変更前RUN0011の呼出し診断（補足）

RUN0011のCore trialは`service_calls=2232`、`service_bytes=4998`で、平均は`4998 / 2232 = 2.24 byte/call`だった。変更前にはbyte/frame/time budgetは存在せず、budget hitは0として扱うべき状態だった。

コード上の確認結果:

- `available()==0`ならその呼出しは読み出しなしで戻る。したがってservice_callsは、データがない周期も含む。
- `available()>0`の間は1 byteずつdecoder.feed()し、delimiter到達時にフレーム処理、cache、TimeSync、queue投入まで同じloopTaskで実行していた。
- UART受信ループ内でSD書込み、JSON生成、USBシリアル出力は行っていない。
- queue投入のcritical sectionは短時間だが、decoder、cache、TimeSync、非BNO enqueueなどのフレーム処理はloopTaskの処理時間を消費する。
- serviceControl()自身がmutexを長時間保持して待つ構造ではない。従って、4998 byteに留まったことを「意図したbudgetで制限した」とは説明できない。
- WebServer/M5.update/GNSS/TX/drawも同じloopTaskにあり、受信処理と競合する。専用RX taskではdecoder状態を1 taskに限定し、loopTaskとの公平性を分離した。

Wi-Fi/LwIP内部taskのCore・priorityはESP-IDF/framework管理で、アプリ側コードから取得できないため推測値を記録していない（`framework_unavailable`）。アプリのWeb処理は`loopTask`の`web.handleClient()`で実行される。 
## 2026-08-02 BNO 5レポート経路確認とRUN0016/17・RUN0024/25/26の確定報告

### 1. RUN0016/17 UART整合性

RUN0016はdecode_count、RUN0017はdispatch_no_bno_enqueueで、どちらもbno_log_enqueue=0の10秒試験である。CRC、COBS、length、unknown、RX buffer fullはいずれも0だった。

|項目|RUN0016|RUN0017|
|---|---:|---:|
|Core受信bytes（TRIAL_END）|253,572|253,000|
|Core decoded frames|2,640|2,637|
|sequence gap|12|6|
|CRC/COBS/length|0/0/0|0/0/0|
|unknown|0|0|
|BNO kind 1/2/3/4/5|630/500/0/250/0|632/500/0/250/0|
|RX buffer full|0|0|
|XIAO BNO TX drop/partial/zero|0/0/0|0/0/0|

RUN0016のXIAO側TXTYPE合計encoded bytesは253,572 byte、RUN0017は253,072 byteである。Core側のTRIALTYPE合計は、それぞれTRIAL_ENDの253,572 byte、253,000 byteと一致する。型別の保存値は次の通り。

RUN0016（XIAO / Core、件数、encoded bytes）:
- BnoAccel: 630 / 52,920、630 / 52,920
- BnoGyro: 500 / 42,000、500 / 42,000
- TofFrame: 47 / 10,528、48 / 10,752
- VescStatus: 429 / 37,752、433 / 38,104
- TimeSyncReply: 10 / 560、11 / 616
- GnssProcessResult: 99 / 9,504、100 / 9,600
- BnoMagnetic: 250 / 21,000、250 / 21,000
- EstimatedState: 99 / 14,652、100 / 14,800
- PrimaryImuSnapshot: 195 / 17,940、197 / 18,124
- Heartbeat: 100 / 4,000、101 / 4,040
- EskfState: 195 / 35,880、198 / 36,432
- EskfInnovation: 21 / 1,512、22 / 1,584
- EskfHealth: 50 / 3,600、50 / 3,600

RUN0017（XIAO / Core、件数、encoded bytes）:
- BnoAccel: 632 / 53,088、632 / 53,088
- BnoGyro: 500 / 42,000、500 / 42,000
- TofFrame: 48 / 10,752、47 / 10,528
- VescStatus: 426 / 37,488、430 / 37,840
- TimeSyncReply: 10 / 560、11 / 616
- GnssProcessResult: 98 / 9,408、99 / 9,504
- BnoMagnetic: 250 / 21,000、250 / 21,000
- EstimatedState: 99 / 14,652、99 / 14,652
- PrimaryImuSnapshot: 196 / 17,940、197 / 18,124
- Heartbeat: 99 / 3,960、100 / 4,000
- EskfState: 195 / 35,880、197 / 36,248
- EskfInnovation: 22 / 1,584、25 / 1,800
- EskfHealth: 50 / 3,600、51 / 3,672

元のRUN0016/17取得時はTRIAL_SEQUENCEおよびP1_CAPTURE_SEQUENCEをまだ出力していなかったため、Core/XIAOのfirst sequence、last sequenceは保存されていない。aggregateのsequence gapから元データを逆算することはできない。この欠落を埋めるため、以後のRUN0024〜0026では両端sequenceを明示的に保存した。

判定として、RUN0016/17はフレーム形式のエラー、unknown、RX overflow、XIAO送信dropはなく、BNO型別の受信値も一致している。一方、sequence gapはRUN0016=12、RUN0017=6であり、ユーザー指定の「sequence gap=0」を満たさないため、UART integrity passとは確定しない。

### 2. BNO08Xコード上のreport対応

実装箇所は xiao-boat-control-integration/src/bno_reader.cpp の reportKind、enableOne、enableReports、onSensorEvent、handle、poll である。enableReportsが各SH-2 reportをenableOne経由で登録し、onSensorEventで受信イベントを数え、handleのsensorId switchでkindへ変換している。pollで sensor_.wasReset() を検出した場合はcallback再登録と全report再設定を行う。

|kind|センサ|SH-2 sensor ID / report ID|interval|要求Hz|
|---:|---|---:|---:|---:|
|1|Accelerometer|SH2_ACCELEROMETER=1|20,000 us|50|
|2|Gyroscope Calibrated|SH2_GYROSCOPE_CALIBRATED=2|20,000 us|50|
|3|Game Rotation Vector|SH2_GAME_ROTATION_VECTOR=8|20,000 us|50|
|4|Magnetic Field Calibrated|SH2_MAGNETIC_FIELD_CALIBRATED=3|50,000 us|20|
|5|Linear Acceleration|SH2_LINEAR_ACCELERATION=4|20,000 us|50|

BOAT_EXPERIMENT=18はkind3のみ、19はkind5のみ、20はkind1〜5全てをenableする。enableOneはsensor_.enableReport(reportId, intervalUs)の戻り値をenableSuccess/enableFailureへ加算する。各試験のenable_ok=2は初回設定とwasReset後の再設定を含み、reset_reconfig=1である。

イベント経路カウンタは次の意味で分離されている。

- callback: BNOライブラリのコールバック到着数
- switch: sensorId switchに入った回数
- kind_convert: sensorIdからkindへの変換成功数
- events: kind別に処理できたBNOイベント数
- tx_event: BNOイベントをTX処理へ渡した数
- tx_req / tx_enq / tx_done: UART送信要求、送信キュー投入、送信完了
- tx_drop / event_drop: 送信drop、BNOイベントキューdrop
- Core receive: Core TRIAL_ENDのbno_kind別受信数
- effective_hz: first/last sensor timestampからの実効周波数

BNOイベント数とUART送信完了数は同じ意味ではない。これらを別カウンタで保持し、event→TX要求→enqueue→completed→Core receiveの各段階を個別に比較できるようにした。I2C error/reinit、wasReset/reconfigも別系統で記録する。

### 3. 個別10秒試験

#### RUN0024 / BNO_K3_game_rotation_10s

Core受信はbno_kind=0/0/506/0/0、sequence first=17,586、last=19,294、sequence gap=6、CRC/COBS/length/unknown/RX full=0。XIAOはkind3 report_id=8、interval=20,000 us、requested=50 Hz、callback/switch/kind_convert/events/tx_event/tx_req/tx_enq/tx_done=506/506/506/506/506/506/506/506、tx_drop=0、event_drop=0、effective_hz=50.013、reset_reconfig=1。XIAO sequenceはfirst=17,587、last=19,281。BINは507 records、trailing=0で全件復号、TXTはrecords=507、queue_drops=0、sd_write_errors=0、control CRC/COBS/length=0/0/0、SDTRACEの512 B write、flush、close、TXT生成は成功した。BNOイベント経路は成立したがsequence gap=6のためUART完全合格ではない。

#### RUN0025 / BNO_K5_linear_accel_10s

Core受信はbno_kind=0/0/0/0/506、sequence first=5,054、last=6,766、sequence gap=0、CRC/COBS/length/unknown/RX full=0。XIAOはkind5 report_id=4、interval=20,000 us、requested=50 Hz、callback/switch/kind_convert/events/tx_event/tx_req/tx_enq/tx_done=506/506/506/506/506/506/506/506、tx_drop=0、event_drop=0、effective_hz=50.020、reset_reconfig=1。XIAO sequenceはfirst=5,056、last=6,750。BINは472 records、trailing=0で全件復号、TXTはrecords=472、queue_drops=0、sd_write_errors=0、control CRC/COBS/length=0/0/0、SDTRACEの512 B write、flush、close、TXT生成は成功した。指定されたK5判定条件を満たす。

#### RUN0026 / BNO_ALL_5reports_10s

Core受信はbno_kind=638/506/506/253/506、sequence first=1,678、last=5,473、sequence gap=14、CRC/COBS/length/unknown/RX full=0。XIAO sequenceはfirst=1,678、last=5,459。最終TXDIAGではkind1/2/3/4/5のBNO event、tx_done、Core receiveはそれぞれ638/638/638、506/506/506、506/506/506、253/253/253、506/506/506で一致し、全kindのtx_drop/event_drop=0、reset_reconfig=1だった。

ALLの実効Hzはkind1=63.062、kind2=50.011、kind3=50.022、kind4=25.006、kind5=50.022。kind3/5は要求値付近だが、kind1/4は要求50/20 Hzから外れている。またsequence gap=14のため、ALLは指定判定を満たさない。BINは470 records、trailing=0で全件復号、TXTはrecords=470、queue_drops=0、sd_write_errors=0、control CRC/COBS/length=0/0/0、512 B write、flush、close、TXT生成は成功した。

### 4. 現在の判定と次の順序

K5単独は合格。K3はBNOイベント経路は合格だがsequence gapで不合格。ALLはBNO型別の経路一致とSD/BIN/TXTは合格だが、kind1/kind4の実効Hzとsequence gapで不合格である。

したがって、まだBNO enqueue=trueのdispatch 10秒試験および60秒静止試験へは進めない。次はsequence gapの発生源と、ALL時のkind1/kind4実効周期を追加変更なしの診断で確認する。条件を満たした後にのみ、BNO enqueue=trueの10秒試験、続いて完全静止60秒試験を実施する。

保存先:
- RUN0024: pc-tools/boat_eskf/captures/BNO_K3_REBASE_RUN0024_20260802/
- RUN0025: pc-tools/boat_eskf/captures/BNO_K5_REBASE_RUN0025_20260802/
- RUN0026: pc-tools/boat_eskf/captures/BNO_ALL_REBASE_RUN0026_20260802/
- RUN0016/17の元ログ: pc-tools/boat_eskf/captures/UART_RX_DECODE_COUNT_CURRENT2_LOG10_20260802/、pc-tools/boat_eskf/captures/UART_RX_DISPATCH_CURRENT3_LOG10_20260802/