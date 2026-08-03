# 2026-08-03 CONTROL_LINK_CHECK2 / RUN0055

## 目的
RUN0054で発生したXIAO Bnoタスクのstack canary再起動を、通信条件を変えずに再確認した。対象はボート用XIAO COM4（MAC 34:85:18:AB:FA:90）とCoreS3 COM6（MAC 30:ED:A0:D4:BF:40）のみで、COM3は未接続・未操作。

## 実施前の修正
XIAO `bnoTask` のFreeRTOSスタックを4096 wordから8192 wordへ増量した。理由は、Bnoタスク内の既存ESKF 15状態共分散伝播（複数の15x15自動行列）と診断スナップショット／trace呼出しの合成スタックが4096 wordを越え、RUN0054でstack canary再起動したため。BNO周期、UART、ESKF演算、SD、mutex、ログ形式、アクチュエータ設定は変更していない。

## ビルド・書込み
- XIAO環境 `bno_final_all_100_100_20_50_50_10s`: build成功（RAM 204308/327680、Flash 576429/3342336）、COM4書込み・hash verified。
- Core環境 `m5stack-cores3`: build成功（RAM 168076/327680、Flash 1032377/6553600）、COM6 ROM no-stub/DIO書込み・hash verified。

## 短時間確認
POST `/api/log/start?duration_s=1&bno_capture=1&bno_log_enqueue=0&uart_rx_diag=dispatch&eskf_reset_at_start=1` はHTTP 202、`eskf_reset_queued=true`。RUN番号はRUN0055。

- XIAO `P1_START_ACK` と `P1_STOP_ACK`: 各1。
- Core `TRIAL_START_ACK` と `TRIAL_STOP_ACK`: 各1。
- Core trial: `seq_gap=0`、CRC/COBS/length=0/0/0、RX buffer full=0、BNO kind 1..5 = 127/100/50/25/50。
- XIAO TX: kind 1..5のenqueue/doneは127/100/50/25/50、drop=0、event queue drop=0。
- ESKF API: `reset_count=1`、`shadow_only=true`、`actuator_output_enabled=false`、`xiao_link.sequence_gaps=0`、CRC/COBS/length=0/0/0。
- logger: `enqueue_accepted=115`、`queue_dequeued=115`、`written_to_bin=115`、`pending_in_queue=0`、`LOGGER_FINAL state=FINALIZED q=0`、flush/close/summary TXT open/write/close成功。RUN0055.BIN=16758 bytes、TXT=253 bytes。
- stack overflow/Guru Meditation/reboot: Core/XIAO双方0件。RUN0054のSTOP ACK timeoutは再現しなかった。
- 参考診断: loggerの短時間試験では`partial_writes=1`が記録されたため、10秒合格判定では512 B書込み全件とpartial/zeroを改めて確認する。これは通信確認を不合格とするものではない。

## 証跡
`pc-tools/boat_eskf/captures/CONTROL_LINK_CHECK2_20260803/` に開始応答、ESKF/link/files API応答、Core/XIAO生シリアル、capture_metaを保存。RUN0055は短時間通信確認用であり、10秒raw BNO試験の結果ではない。

## 判定と次段階
START/STOP/ACK、ESKF reset、finalize、再起動なしの短時間通信条件は合格。次は同じファームウェア・同じ周期・同じ安全条件で、要求された10秒試験を1回だけ実施し、kind 1～5のevent→TX enqueue→TX complete→Core受信→BIN保存の段階差、partial/zero write、queue drop、BIN復号を確認する。合格するまで通常運用周期へ切り替えない。
