# 提言書向けリプレイ試験仕様（2026-08-04）

## 入力

`proposal_replay.py`の`ReplayEvent(t_ms, kind, value)`を使う。現時点では保存済み実ログを直接再生する代わりに、既存通信・センサ周期を模した決定的入力を使用する。実ログ接続時も同じイベント列へ変換して比較できる構造とした。

## 正常シナリオ

次のイベント列を固定する。

`STOPPED → INITIAL_PLACED → START → LAUNCH_ALIGN → START_RAMP → GNSS_SPEED_UP → COG_VALIDATING → LOS_NAVIGATION → WAYPOINT_1 → WAYPOINT_2 → GOAL_STOP`

同じイベント列を2回実行し、状態遷移、SHADOW出力、最終状態がJSONとして一致することを再現性条件とする。

## 異常シナリオ

GNSS stale、一時的COG不安定、GVR欠落、BNO停止、ToF無効値、UART sequence gap、Heartbeat断、STOP、E-STOP、SD書込み異常、NaN/Inf、timestamp飛びを個別に注入する。

異常イベント後は状態機械を`DISARMED`または`E_STOP`へ遷移させ、SHADOW出力を`(0,0,0)`にする。これは実アクチュエータ出力ではなく、出力生成前の安全判定結果である。

## 合格条件

* 正常シナリオの必須フェーズを網羅
* 同一リプレイの結果が再現可能
* 全SHADOW出力が有限値
* STOP/E-STOP/Heartbeat断/異常後の出力がゼロ
* 異常カウンタが該当イベントを1以上記録

この仕様で確認できるのは決定性と状態遷移のみで、XIAO実時間deadline、UART実帯域、SD実書込みは確定できない。
