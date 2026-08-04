# 次段階XIAO SHADOW試験手順（2026-08-04）

今回のホスト試験とビルドが成功した後にのみ実施する。実機操作は別承認とし、COM3は対象外である。

1. `origin/main`から専用試験ブランチを作成し、XIAO制御側COM4、通信側CoreS3 COM6を個別確認する。CoreS3シリアルを開かず、必要ならAPIを使う。
2. `proposal_replay_min`相当のイメージを選び、`ACTUATOR_OUTPUT_ENABLE=0`、`shadow_only=true`、VESC/PCA9685未接続または安全状態を確認する。
3. 既存のBNO/ToF/GNSS/UART/SD形式を変更せず、60秒のSHADOW試験を1回だけ実施する。
4. タスク周期・実行時間・deadline miss、stack high-water、heap、queue、UART gap/drop、SD write/block/error、センサage、I2C、watchdog、再起動、NaN/Inf、SHADOW飽和を保存する。
5. 同条件でMIN、MID、FULLを個別に比較する。全ての設定でSTOP、E-STOP、Heartbeat断を含む安全応答を確認する。
6. 60秒で問題がなければ、同一設定を10分へ延長する。アクチュエータ接続、VESC指令、水上試験はこの手順の範囲外である。

実機結果が得られるまで、A/B/C/Dの最終判定と大会採用判断は保留する。

## 2026-08-04 ソフトウェア実装結果（最新）

最新方針に従い、実機書込み・COM操作・センサ試験は行わず、Draft PR #18ブランチへ実時間MIN経路を実装した。`shared/proposal_min`を追加し、GNSS局所NED変換、waypoint、LOS/launch yaw、COG妥当性、roll PD、ToF中央値/傾き補正/LPF、高さP、左右前翼＋後部ヨー＋推進shadow、安全状態を固定長で接続した。`proposal_shadow_min`はBOAT_EXPERIMENT=23、SHADOW_CONTROL_ENABLE=1、ACTUATOR_OUTPUT_ENABLE=0、PROPOSAL_PROFILE=1である。

PCA9685/VESCの全出力経路はコンパイル定数と乾式ランタイム条件で二重遮断し、通信側にも同じ出力禁止static_assertと`proposal_shadow_comm`環境を追加した。計測構造体にはtask/operation、queue/UART、sensor age、SD/I2C/heap/watchdog予約、NaN/Inf、STOP/E-STOP/heartbeat、saturation、SHADOW出力count/min/maxを追加した。

検証はC++単体PASS、Python unittest 11件PASS、ホストbenchmark/replay全モード有限値・再現性PASS、XIAO通常環境PASS、XIAO proposal_shadow_min PASS、通信側proposal_shadow_comm PASS、CoreS3既存環境PASS。これらは静的/ホスト検証であり実機合格ではない。実機計測の未接続項目は`docs/PROPOSAL_MIN_UNMEASURED_20260804.md`に記載した。