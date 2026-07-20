# 制御側 XIAO 統合ファームウェア

制御側はBNO08X、VL53L5CX、INA226、PCA9685、VESCを扱い、D6/D7の921600 bps UARTで通信側へテレメトリを送信します。GNSS、microSD、Wi-Fi/Web UIは通信側XIAOの役割です。

通信側から受信する `STOP` は出力を停止してDISARMEDへ、`E-STOP` は出力を即時停止してE_STOPへ遷移させます。制御側の一時的なWi-Fi診断は無効化済みです。

実機への書き込みは、二台の配線と電源状態を確認してからこのプロジェクトで行います。
