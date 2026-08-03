# BNO UART分離10秒診断 RUN0011 詳細報告（2026-08-02）

## 結論

対象RUN0011は、SD書込みとfinalizeは成功しましたが、BNO生データのCoreS3受信・復号は不成立です。XIAO側ではBNO送信要求、キュー投入、UART完了が成立しているため、ログ生成・ログキューではなく、CoreS3のUART受信処理またはdecoder処理の飽和が主判定です。指定分岐はBです。60秒試験およびBNO静止性能試験には進みません。

なお、同じシリアル取得中に先行して受理されたRUN0010の診断ブロックも含まれています。RUN0011は今回のcurl開始応答と対応する対象RUNとして個別に抽出しました。RUN0010は今回の判定対象外です。

## 条件

- CoreS3: m5stack-cores3-telemetry-bridge 診断版
- XIAO: xiao-boat-control-integration 診断版
- UART: XIAO側リンク921600 bps、USBシリアル取得はCore COM6/XIAO COM4を115200 bpsで取得
- Core UART: GPIO8 RX、GPIO9 TX
- UART RXバッファ: 16 KiB
- SD SPI: 10 MHz
- LCD: logging中およびlogFinalizing中はdrawを呼ばない。M5.updateは維持
- shadow_only=true、actuator_output_enabled=false、アクチュエータ出力なし
- 開始API: POST /api/log/start?duration_s=10&bno_capture=1&bno_log_enqueue=0
- 開始前5秒、試験終了・finalize後10秒以上のシリアルを保存

## 事前接続確認

- Coreアプリ診断行: 15行以上を開始前に取得
- XIAO診断行: 80行以上を開始前に取得
- 生シリアル保存: Core/XIAOとも実施
- シリアルreader error: Core 0、XIAO 0
- 開始API送信時もシリアル接続を維持
- 開始API応答: HTTP 202
- 応答本文: logging=true、duration_s=10、bno_capture=true、bno_log_enqueue=false
- Core/XIAOの再起動は試験中に観測されず、Core診断reset_reason=0

## RUN番号と保存先

- 対象RUN: RUN0011
- 保存先: pc-tools/boat_eskf/captures/BNO_UART_DECOUPLED_ENQ0_LOG10_20260802_RERUN4/
- BIN: RUN0011.BIN（64,539 byte）
- TXT: RUN0011.TXT（253 byte）
- Core生シリアル: core_serial_raw.log（48,081 byte）
- XIAO生シリアル: xiao_serial_raw.log（146,529 byte）
- 開始応答: start_response.json、start_response.body、start_response.headers
- 状態取得: eskf_04s/08s/12s/16s/20s
- ファイル一覧・ダウンロード応答: files_after、run_bin、run_txt

## Core受信診断（RUN0011試験区間の差分）

- TRIAL_START: bno_log_enqueue=0
- TRIAL_END: rx_bytes=4,998
- serviceControl処理byte: 4,998
- decoded frame: 45
- 有効BNO frame: 0
- BNO kind 1/2/3/4/5: 0/0/0/0/0
- CRC/COBS/length error: 0/0/0
- sequence gap: 0
- unknown type: 0
- BNO kind不正: 0
- RX buffer最大: 16,384 byte
- RX buffer full観測: 12回（finalize後の診断では16回まで増加）
- serviceControl呼出し: 2,232回（TRIAL_END時点）
- 試験中のRXDETAILはおおむね500 byte/s、約4 frame/s
- uart_overflow: framework_unavailable（取得APIなし）

試験区間の差分カウンタだけを見るとCRC/COBS/lengthは0ですが、XIAO送信量に対しCore処理量が大幅に不足し、16 KiBバッファfullが発生しています。finalize後の追加診断では累積COBS=8、length=11、sequence gap=465まで増加し、残留受信データの処理遅延・崩れが確認されました。

## XIAO送信診断（RUN0011のP1_CAPTURE_STATS_RESET以降）

TXはlinkService内の単一UART TX経路に集約されています。Coreリンク用linkUart.writeはlinkServiceだけにあり、VESC用vescUart.writeは別UARTです。

- BNO kind 1（Accelerometer）: event/request/enqueue/done=630/630/630/630、encoded bytes=52,920
- BNO kind 2（Gyroscope Calibrated）: 500/500/500/500、42,000 byte
- BNO kind 3（Game Rotation Vector）: 0/0/0/0、0 byte
- BNO kind 4（Magnetic Field Calibrated）: 250/250/250/250、21,000 byte
- BNO kind 5（Linear Acceleration）: 0/0/0/0、0 byte
- BNO送信drop: 0
- TX queue drop: 0
- partial write: 0
- zero write: 0
- TX queue high-water: 5
- write最大時間: 試験終盤141 us（終了後診断の最大180 us）
- free heap: 258,816 byte、試験中のmin free heapも258,816 byte

kind 3/5はコード上では20,000 us周期でenableReportしているものの、今回の実測送信は0件です。したがって、Core側kind別受信数との一致条件を満たしません。

試験停止直前のXIAO総encoded bytesは約263,584 byteで、10.002秒換算で約26.3 kbyte/sです。8N1実線上の占有率は、XIAO送信実測 26,353 B/s × 10 / 921,600 × 100 = 28.6% である。従って帯域上限（100%）には近くない。

## SD・ログ・finalize

- 512 byte writer write: 保存されたSDTRACE上の全writer writeはok=1、req=512、actual=512
- RUN0011で観測されたwriter write: 10回
- sd_write_errors: 0
- queue drop: 0
- queue high-water: 12
- finalize時q: 0
- flush: ok=1
- close: ok=1
- TXT summary open/write/close: すべてok=1
- TXT records: 489
- 自動停止: normal_stop=1
- BIN復号: 489件を全件復号、trailing byte=0
- BIN queue timestamp span: 10,002,229 us（約10.002秒）
- BIN内の重複sequence: 0、sequence gap: 86。これはbno_log_enqueue=0でBNOレコードを意図的に生成しないためのログsequence飛びであり、UART decoderのtrial差分sequence gapとは別です。

## ESKF・Web

- 自動停止後の/api/eskf: HTTP応答継続
- shadow_only=true
- actuator_output_enabled=false
- state_received/health_received/baseline_received=true
- health_reason=mount_unvalidated
- ESKF reset_count=0
- files_after: HTTP 200、sd_ready=true、logging=false
- RUN0011.BIN/TXTダウンロード: ともにHTTP 200
- ping保存はWindows pingコマンドが5秒でタイムアウトしたためICMP成功とは判定しません。HTTP APIは正常応答しています。

## 判定

SD、writer、flush、close、TXT、BIN復号は正常です。XIAO側送信キューにもdrop・partial writeはありません。一方、XIAOが送ったBNOフレーム数に対しCoreの試験区間有効BNO受信は0件、RX bufferは16 KiBに到達しfull観測が12回、試験後にCOBS/length/sequence累積エラーが増えました。

したがって今回の主原因候補は、bno_log_enqueueやSD writerではなく、CoreS3側のUART受信取り込み、serviceControlの時間予算、decoder状態、または同一Core上の処理競合です。分岐Bとして扱います。分岐D（ログqueueが残ってwriter停止）ではなく、分岐A（最初のSD write失敗）でもありません。

## 次の作業

1. 60秒試験とBNO軸試験は保留。
2. UART受信のFreeRTOSタスク分離または受信取り込み経路の最小診断を、SD設定・ログ形式・BNO周期を変えずに設計する。
3. Core側で受信byteとserviceControl処理byteの差、RX buffer full発生時刻、decoder delimiter状態をさらに保存する。
4. kind 3/5がXIAO BNOコールバックに実際に到達しているかをXIAO側で確認する。ただし今回の主判定はまずCore RX飽和。
5. UART受信が全kind一致・buffer非飽和・trial error=0になってから、bno_log_enqueue=1の10秒試験へ戻る。

## 主要証拠

- RUN0011.BIN / RUN0011.TXT
- core_serial_raw.log（TRIAL_START/TRIAL_END、RXDIAG/RXDETAIL、SDTRACE全行）
- xiao_serial_raw.log（TXDIAG、BNO_TX、P1_CAPTURE_STATS_RESET）
- start_response.json
- files_after.json
- eskf_04s.json、08s、12s、16s、20s
- capture_meta.json