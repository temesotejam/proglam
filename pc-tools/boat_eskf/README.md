# boat_eskf\n\n標準ライブラリだけで動く、CoreS3 BINログ復号・合成入力・SHADOW ESKF比較用リファレンスです。`python -m unittest discover -s tests` でテストできます。\n

## 提言書向け固定長ベンチマーク／リプレイ
実行: `python tools/run_proposal_evaluation.py --iterations 10000 --out-dir results/proposal_20260804`
結果: `benchmark.json`、`replay.json`、`summary.json`。計測時間はホストPC時間であり、XIAO実時間性能を意味しない。
構成: BASE/MIN/MID/FULL/LEGACY。正常フェーズとGNSS stale、COG不安定、GVR/BNO停止、ToF無効、UART gap、Heartbeat断、STOP/E-STOP、SD異常、NaN/Inf、timestamp飛びを再生する。