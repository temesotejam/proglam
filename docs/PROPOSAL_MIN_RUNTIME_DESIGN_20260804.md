# Proposal MIN 実時間経路（2026-08-04）

`shared/proposal_min` は固定長・動的確保なしの SHADOW 計算モジュールである。GNSS緯度経度を局所NEDへ変換し、waypoint管理、LOS/launch yaw、COG妥当性、roll PD、ToF有効ゾーン中央値、姿勢傾き補正、低域通過、高さP、翼合成、安全状態、SHADOW出力を1ステップで実行する。

出力名は `leftFront`、`rightFront`、`rearYaw`、`propulsion`。実機アクチュエータへは接続しない。`propulsion` は常に0で、翼・後部ヨー機構を独立に観測できる。既存のrudder/elevon名称は使用しない。

`xiao-boat-control-integration` の `proposal_shadow_min` 環境では `BOAT_EXPERIMENT=23`、`SHADOW_CONTROL_ENABLE=1`、`ACTUATOR_OUTPUT_ENABLE=0`、`PROPOSAL_PROFILE=1` を設定し、`runProposalMinShadow()` が既存センサ状態から実時間経路を呼び出す。通常環境では従来制御を変更しない。

安全遷移は DISARMED→RUNNING（START）、STOP→DISARMED、E-STOP→E_STOP、heartbeat timeout/入力不正→FAULT。RUNNING以外は全SHADOW出力を0にする。
