# PR #18 最終監査報告（2026-08-04）

対象: `feat/proposal-benchmark-replay-20260804` / Draft PR #18
判定ラベル: 「実装済み・ホスト確認済み」「実装済み・ビルド確認済み」「実装済み・実機未確認」「ハードウェア不在により保留」

1. **結論 — 実装済み・ホスト確認済み**
   MIN SHADOWの安全ガード、Waypoint共通C++実装、Type63–67共通wire、30分決定的fixture、診断デコーダと陰性テストを完了。物理出力は無効のまま。実機試験は行っていない。
2. **初期HEAD / 最終HEAD — 実装済み・ホスト確認済み**
   監査開始時は `af55ae17d6191cba8816cb9b13a8993837c652b6`（`af55ae1`）。最終HEADは本報告を含む監査コミット後に確定し、この項へ追記する。
3. **ブランチ — 実装済み・ホスト確認済み**
   `feat/proposal-benchmark-replay-20260804`。
4. **PR — 実装済み・ホスト確認済み**
   [PR #18](https://github.com/temesotejam/proglam/pull/18)。Draft、open、unmergedを維持する。
5. **main差分 — 実装済み・ホスト確認済み**
   mainへのpush、merge、rebase、squash、force pushは行わない。mainは変更しない。
6. **現行ファームウェア変更 — 実装済み・ビルド確認済み**
   制御側WaypointSetをshared実装へ接続し、DISARMED以外を拒否、ACKを一回返し、Accepted時だけatomic commit。停止系出力を即時safeへ固定。VESC受信時刻をType65へ反映。
7. **host dummy値の除去 — 実装済み・ホスト確認済み**
   30分試験はController::stepとproduction `boat_protocol.h`のencode/decodeを使用。Type66/67は実体payloadとCRCを生成し、zero-array dummyを使用しない。host試験限定でpropulsion範囲0..1と0.4 commandを使用し、firmware設定は0..0のまま。
8. **Type63–67経路 — 実装済み・ホスト確認済み**
   Type63/64/65/66/67をproduction共通structでencode、COBS/CRC decode、長さ・Waypoint CRC・ACK statusを検査。wire sizeは190/32/48/76/16 bytes。
9. **C++ Waypoint 6状態試験 — 実装済み・ホスト確認済み**
   BOOT、DISARMED、ARMED_IDLE、RUNNING、E_STOP、FAULTを検査。DISARMEDのみAccepted。
10. **reject atomicity — 実装済み・ホスト確認済み**
    reject/duplicate/NaN入力後もrevision/count/point/active indexを変更しないことを確認。
11. **30分周期・回数 — 実装済み・ホスト確認済み**
    20,000 us周期、1,800 s、90,000 cycles、90,000 ControlSnapshot、全270,002 records。
12. **状態・イベントシナリオ — 実装済み・ホスト確認済み**
    START/DISARMED、Waypoint revision/index、STOP、明示restart、E_STOP、heartbeat timeout、GNSS/IMU/ToF fault、IMU NaNをfixtureで実行。starts=8、stops=7、E_STOP samples=500、heartbeat fault samples=500、sensor fault samples=1550。
13. **4出力範囲 — 実装済み・ホスト確認済み**
    left front/right front/rear yaw/propulsionを記録。RUNNING中propulsion非zero=73,000、safe zero=17,000、範囲違反=0。
14. **STOP safe/restart — 実装済み・ホスト確認済み**
    STOP/E_STOP/FAULT/非RUNNINGは4出力をneutral/stopへ即時固定。明示STARTなしの再開はデコーダで検出。stop_restart違反=0。
15. **INA/VESC temporal join — 実装済み・ホスト確認済み**
    control timestampより未来のsampleをjoinせず、sample age/staleを記録・診断。正常fixtureのfuture join=0、陰性fixtureで検出を確認。
16. **5センサ stale — 実装済み・ホスト確認済み**
    GNSS、IMU、ToFおよびjoined INA/VESCのstale条件を診断。正常fixtureのstale違反=0、陰性fixtureでカウンタ増加を確認。
17. **安全理由 — 実装済み・ホスト確認済み**
    E_STOP、heartbeat、sensor fault、非finite、安全出力を状態・reasonと照合。正常fixtureのmismatch=0。
18. **negative tests — 実装済み・ホスト確認済み**
    stale、NaN、invalid state transition、range、safe output、slew、STOP restart、future join、sequence gap、timestamp reversal、version、unknown type、payload length、Waypoint CRC/statusを陰性fixtureで各検出。
19. **Type record生成/復号数 — 実装済み・ホスト確認済み**
    Type63/64/65/66/67を生成し、production decoderで全件復号。payload length/CRC/status error=0。
20. **sequence/timestamp/version/unknown — 実装済み・ホスト確認済み**
    正常fixtureはsequence gap=0、timestamp reversal=0、version/unknown=0。
21. **決定性SHA256 — 実装済み・ホスト確認済み**
    2回生成結果が一致: `52786643133850F67651F0858E2834410ADC622DDD17FF84D1461C24C9EE47D8`。
22. **C++試験 — 実装済み・ホスト確認済み**
    `WAYPOINT_APPLY_HOST_PASS states=6 rejected_non_disarmed=5 accepted_disarmed=1 duplicate=1 invalid_nan=1 atomicity=ok`。30分試験も2回PASS。
23. **Python試験 — 実装済み・ホスト確認済み**
    `python -m unittest discover -s pc-tools/boat_eskf/tests -q`: 15 tests PASS。
24. **compileall — 実装済み・ホスト確認済み**
    `python -m compileall -q pc-tools/boat_eskf` PASS。
25. **3ビルド — 実装済み・ビルド確認済み**
    `proposal_shadow_min`、`proposal_shadow_comm`、`m5stack-cores3`をPlatformIOでSUCCESS。RAM/Flash値は既存のビルド記録に保存。
26. **SHADOW/physical output — 実装済み・ビルド確認済み**
    `SHADOW_CONTROL_ENABLE=1`、`ACTUATOR_OUTPUT_ENABLE=0`。制御式・ゲイン・MID/FULL/ESKF/UART/SD/SPI/mutexは変更しない。
27. **INA — 実装済み・ビルド確認済み**
    実機firmwareのINA226取得は無効設定を維持。host fixtureのINAは診断経路確認用であり実機有効化ではない。
28. **hardware/COM — ハードウェア不在により保留**
    COM3を含むCOM操作、書込み、センサ試験、モータ起動、実機出力は行っていない。
29. **文書更新 — 実装済み・ホスト確認済み**
    本報告、`docs/MIN_SHADOW_OPERATION_SPEC_20260804.md`、`docs/WORK_PLAN.md`、`docs/WORK_LOG.md`、既存の`詳細な報告.md`を更新する。
30. **文字化け対応 — 実装済み・ホスト確認済み**
    既存報告末尾の文字化けを上書きせず、UTF-8の本報告を新規作成した。
31. **変更ファイル — 実装済み・ホスト確認済み**
    `shared/proposal_min/src/waypoint_apply.{h,cpp}`、`shared/proposal_min/src/proposal_min.cpp`、`xiao-boat-control-integration/src/main.cpp`、`pc-tools/boat_eskf/cpp_tests/{Arduino.h,waypoint_apply_host.cpp,min_shadow_long_host.cpp}`、`pc-tools/boat_eskf/boat_eskf/min_shadow_log.py`、`pc-tools/boat_eskf/tests/test_min_shadow_negative.py`。
32. **追加コミット — 判断待ち**
    監査実装と報告を既存 `af55ae1` にamendせず、追加コミットとして保存する。
33. **push — 判断待ち**
    feature branchのみoriginへpushする。mainへのpush/mergeはしない。
34. **未解決 — ハードウェア不在により保留**
    30分実機連続記録、実際のPCA/VESC/INA出力、microSD、UART負荷、センサ実測、実機BIN完全復号は未確認。正しい対象機材が明示的に使用可能になった後に別試験として実施する。

## Appendix: host evidence

```
MIN_SHADOW_LONG_PASS duration_s=1800 period_us=20000 steps=90000 outputs=90000 starts=8 stops=7 estops=500 heartbeat_fault_windows=500 sensor_fault_samples=1550 running_nonzero_propulsion=73000 safe_zero_outputs=17000 transitions=21 nan_inf=0 deadline_miss=0 waypoint_ack=accepted
```