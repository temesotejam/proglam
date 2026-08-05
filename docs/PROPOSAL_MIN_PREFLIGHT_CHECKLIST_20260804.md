# MIN SHADOW 実機前チェックリスト

1. COM3を操作対象から除外し、XIAO制御側と通信側のMAC/COMを再確認。
2. `proposal_shadow_min` と `proposal_shadow_comm` のイメージhashを保存。
3. actuator_output_enabled=false、shadow_only=true、E-STOP系をAPI/診断で確認。
4. GNSS waypoint送信、BNO/ToF/INA受信、UART CRC/COBS/length、heartbeatを確認。
5. SD開始前後の生シリアル、BIN/TXT、API応答、task/heap/queue/SD診断を保存。
6. START→RUNNING、STOP→DISARMED、E-STOP→E_STOPを無負荷で確認。
7. 欠落・未計測フィールドが残る場合は60秒試験を開始しない。
