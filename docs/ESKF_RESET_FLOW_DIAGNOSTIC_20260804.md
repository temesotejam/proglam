# ESKFリセット指令経路診断（2026-08-04）

## 結論

RUN0064およびP1試験は開始していない。リセット専用試験を1回だけ実施した結果、CoreS3側のAPI受付・UART送信完了までは成功したが、XIAO側の受信／ACK／ESKF reset_count増加は確認できなかった。XIAO生ログには `loopTask` の stack canary panic があり、アドレス解析でGNSS更新中のESKF処理に到達していることを確認した。したがって、この試験の最初の失敗点はUART CRC/COBS/lengthではなく、XIAO側のスタック枯渇によるタスク停止である。

## 試験条件と対象

- BOAT_EXPERIMENT=23、kind 1 Accelerometer 100 Hz、kind 2 Gyroscope 100 Hz、kind 4 Magnetic Field 20 Hz。kind 3/5は無効。
- Core API使用、actuator_output_enabled=false、shadow_only=true、DRY_RUN。
- XIAO COM4（MAC `34:85:18:AB:FA:90`）、CoreS3 COM6（MAC `30:ED:A0:D4:BF:40`）。COM3は接続・書込み・操作なし。
- 送信したAPIは `POST /api/eskf/reset` を1回のみ。P1 START/STOP、RUN0064、ログ開始は実施していない。

## コード監査

従来の `eskf_reset_queued=true` は、CoreS3の `sendControl()` がフレーム全長を書けたことだけを示し、XIAOでの実行やACKを保証していなかった。XIAOのEskfCommand分岐にもACK送信がなかった。今回、既存のCommandAck型を使用して、Core側のAPI→TX enqueue→TX complete→ACK受信→reset_count観測をAPIとシリアル診断へ追加し、XIAO側に受信・action・CRC/length判定・実行結果ACKを追加した。無線プロトコル、BNO周期、ESKF方程式、mount変換、アクチュエータ設定は変更していない。

## 1回目のリセット専用試験

保存先: `pc-tools/boat_eskf/captures/ESKF_RESET_FLOW_DIAGNOSTIC_20260804/`

| 段階 | 結果 |
|---|---:|
| API受付 | 1 |
| Core TX enqueue | 1 |
| Core TX complete | 1 |
| XIAO ACK受信 | 0 |
| ACK timeout | 1 |
| ACK unexpected | 0 |
| reset_count before/after | 0 / 0 |
| UART sequence gap | 0 |
| UART CRC/COBS/length | 0 / 0 / 0 |

API応答は `{"queued":true,"single_shot":true}`。Coreの `last_command_id=1713`、ACKは未受信である。XIAOログには `ESKF_RESET_FRAME_RECEIVED` 等の受信行はなく、`Guru Meditation Error` と `Stack canary watchpoint triggered (loopTask)` が記録された。

panic backtraceをXIAO ELFでaddr2lineした結果は次の通り。

```text
0x42004ee5  eskf::Shadow::updateLinear(...)  eskf.cpp:97
0x420059fd  eskf::Shadow::updateGnss(...)    eskf.cpp:110
0x42008002  processNav(...)                  main.cpp:105
0x42009a49  linkRxService(...)               main.cpp:157
0x42009eba  loop()                           main.cpp:177
```

これはresetフレームのCRC/COBS/length不良を示す証拠ではなく、XIAOのloopTaskがGNSS/ESKF更新で停止し、ACKを返せなかったことを示す。

## 最小修正と再起動確認

XIAOのArduino loop stackを既定8 KBから16 KBへ拡大する `-DARDUINO_LOOP_STACK_SIZE=16384` だけを追加した。両側をビルドし、CoreS3はCOM6へROM no-stub/DIOで、XIAOはCOM4へ書込み、MACとイメージhashを確認した。COM3には触れていない。

修正後はリセットAPIを再送していない（1回試験の条件を保持）。COM4の起動ログではstack canary/Guru Meditationは再発せず、BNOイベントが継続した。代表値は accel 1778、gyro 1407、mag 351 events（約8秒取得）、TX drop/partial/zeroは0。Core APIもフレーム受信、sequence gap/CRC/length=0で復旧している。XIAOの `FAULT` はCore heartbeat/安全状態によるもので、reset ACK成功を意味しない。

## 判定と次の手順

今回のリセット試験は不成立（ACK未受信、reset_count不変）。ただし原因候補はXIAO loopTask stack overflowまで絞り込めた。次は修正済みファームウェアで、RUN0064/P1を開始せず、リセット専用APIをもう一度だけ送り、XIAO ACKとreset_countの+1を確認する。そこが合格した場合のみRUN0064へ進む。再度ACKが無い場合は、ソフトウェア側の証拠を保ったままUART信号方向・配線を確認する。

## 証跡一覧

- `reset_api_response.json`
- `api_link_before_test.json`, `api_eskf_before_test.json`
- `api_link_after_reset.json`, `api_eskf_after_reset.json`
- `api_link_after_stack_fix.json`
- `xiao_reset_flow_serial.log`
- `xiao_post_stack_fix_boot.log`

