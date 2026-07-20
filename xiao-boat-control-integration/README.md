# 制御側 XIAO 統合ファームウェア

制御側はBNO08X、VL53L5CX、INA226、PCA9685、VESCを扱い、D6/D7の921600 bps UARTで通信側へテレメトリを送信します。GNSS、microSD、Wi-Fi/Web UIは通信側XIAOの役割です。

通信側から受信する `STOP` は出力を停止してDISARMEDへ、`E-STOP` は出力を即時停止してE_STOPへ遷移させます。制御側の一時的なWi-Fi診断は無効化済みです。

GNSS往復試験では通信側から10 Hzで届く`GNSS_NAV`を検証し、DRY_RUNの計算結果を`GNSS_PROCESS_RESULT`として返信します。`kDryRunActuators=true`が既定であり、非ゼロVESC dutyやPCA9685サーボパルスを出力しません。

実機への書き込みは、二台の配線と電源状態を確認してからこのプロジェクトで行います。

## 自動一括ベンチマーク

通信側XIAOが試験指揮役です。`BenchmarkPrepare`では必ず`kDryRunActuators=true`、PCA9685の全チャンネルFull OFF、VESC Duty 0を確認してから、共用I2Cの再初期化、INA226プロファイル、VL53L5CXプロファイルを適用します。BNO08X専用I2Cは100 kHzのまま変更しません。制御側は測定中もDISARMEDのままで、実アクチュエータを動かしません。

INA226はCURRENT（AVG=128、約6.64 Hz）、BALANCED（AVG=16、約53.15 Hz）、FAST（AVG=4、約212.6 Hz）を使います。Mask/EnableのCVRFは読出しでクリアされるため、読取り前のCVRFをfresh、未セットをduplicateとして統計化します。ToFは8x8/10・15 Hz、4x4/15・30 Hzを要求し、拒否時はそのフェーズをFAIL/NOT SUPPORTEDとして返信します。
