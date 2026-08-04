# 未計測項目（実機前）

以下は静的ビルドとホスト試験だけでは確定できない。

- XIAO実機でのMIN task周期、runtime p95/p99、stack high-water、heap最小値。
- GNSS/ToF/BNOの実センサage、I2Cエラー、UART実効bytes/drop/gap。
- CoreS3 SD生成/書込み/flush/close/TXT、BIN復号、queue drop、SDエラー。
- 実配線でのSTOP/E-STOP/heartbeat遷移と安全出力確認。
- 実船の左右前翼・後部ヨー機構の符号、ゲイン、機械限界。

したがって今回の成果は「実機試験可能なソフトウェア経路の実装・ビルド済み」であり、実機合格ではない。
