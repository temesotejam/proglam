# 次段階XIAO SHADOW試験手順（2026-08-04）

今回のホスト試験とビルドが成功した後にのみ実施する。実機操作は別承認とし、COM3は対象外である。

1. `origin/main`から専用試験ブランチを作成し、XIAO制御側COM4、通信側CoreS3 COM6を個別確認する。CoreS3シリアルを開かず、必要ならAPIを使う。
2. `proposal_replay_min`相当のイメージを選び、`ACTUATOR_OUTPUT_ENABLE=0`、`shadow_only=true`、VESC/PCA9685未接続または安全状態を確認する。
3. 既存のBNO/ToF/GNSS/UART/SD形式を変更せず、60秒のSHADOW試験を1回だけ実施する。
4. タスク周期・実行時間・deadline miss、stack high-water、heap、queue、UART gap/drop、SD write/block/error、センサage、I2C、watchdog、再起動、NaN/Inf、SHADOW飽和を保存する。
5. 同条件でMIN、MID、FULLを個別に比較する。全ての設定でSTOP、E-STOP、Heartbeat断を含む安全応答を確認する。
6. 60秒で問題がなければ、同一設定を10分へ延長する。アクチュエータ接続、VESC指令、水上試験はこの手順の範囲外である。

実機結果が得られるまで、A/B/C/Dの最終判定と大会採用判断は保留する。
