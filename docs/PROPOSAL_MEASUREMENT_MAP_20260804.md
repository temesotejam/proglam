# MIN計測マップ（2026-08-04）

`proposal_min::Metrics` に次を固定長で保持する。

- task/operation: calls、total、max、64サンプル、deadline miss、NaN/Inf、invalid、カテゴリ別saturation（平均とmaxはファームウェア診断行で出力、p95/p99は保存サンプルから後処理）。
- queue/UART: current/high-water/drop、bytes/drop/gaps（既存link値を接続）。
- sensor age: GNSS、GVR/attitude、gyro、ToF、INA。I2C、SD生成/書込み/ブロック/エラー、watchdog/reset、STOP/E-STOP/heartbeat、状態遷移、計測オーバーヘッド用フィールドを予約。
- SHADOW output: count、各出力のmin/max、現在値、propulsion=0。

SD内部の生成数・書込み完了数、FreeRTOS task stack high-water、heap最小/最大blockはCore logger側の既存診断が所有し、MIN行では未接続を0/未計測として扱う。未計測値を合格値として解釈しない。
