# PR #18 最終監査是正報告（2026-08-05）

対象ブランチは `feat/proposal-benchmark-replay-20260804`。添付指示に従い、既存履歴を改変せず、制御式・ゲイン・MID/FULL/ESKF/UART/SD/I2C/SPI/mutex/buffer・物理出力設定を変更せずに診断範囲を補強した。正式な4出力名は左前翼、右前翼、後部ヨー機構、単一推進で固定した。

1. **監査開始HEAD**: `a33267e2ab30aed532c48e73c96b5bc8aa36e431`。
2. **試験コードHEAD**: `fd1931c`（是正コード・テストのpushコミット）。
3. **最終push HEAD**: 本報告自身のSHAは自己参照を避け、PR本文/コメントに記録したfeature branch最終tipを正とする。
4. **PR状態**: PR #18はDraft / Open / Unmergedを維持する。
5. **main差分**: mainへのpush・merge・rebase・squash・force pushは行わない。
6. **コミット**: 既存コミットをamendせず、是正用の追加コミットを作成する。
7. **変更ファイル**: `shared/proposal_min/src/proposal_min.cpp`、`pc-tools/boat_eskf/boat_eskf/min_shadow_log.py`、`pc-tools/boat_eskf/cpp_tests/{bin_record_writer.h,min_shadow_long_host.cpp,waypoint_apply_host.cpp}`、`pc-tools/boat_eskf/tests/test_min_shadow_independent_negative.py`、関連文書。
8. **監査ギャップ結果**:

   |項目|結果|
   |---|---|
   |センサ別invalid/stale|GNSS・IMU・ToF、INA、VESCを別カウント。正常データの診断違反0、注入窓は検出|
   |状態・停止理由|状態遷移、明示START、STOP/E-STOP/heartbeat、センサ別reasonを照合。mismatch 0|
   |INA/VESC temporal join|未来sampleをjoinせず、missing/stale/invalid/effective-valid/faultを診断|
   |4出力統計|min/max、non-neutral、changes、safe、range、slew、nonfiniteを集計|
   |Waypoint|6状態、0/1/16/>16、座標・radius境界、revision/atomicity、CRC/ACKをC++で確認|
   |共通BIN|Type63–67は共通protocol encode/decode。ホスト固有magic/writeRecordを共通ヘッダへ移動|

9. **30分周期・回数・時間**: `Controller::step()`、20,000 us周期、1,800 s、90,000 cycles。Python復号CSVは90,000行。
10. **Type別レコード件数**: Type63=90,000、Type64=89,750、Type65=89,750、Type66=1、Type67=1、合計269,502。Type64/65の250件欠落は独立missing窓の注入結果であり、sequence gapではない。
11. **状態遷移**: START=8、STOP=7、E-STOP samples=500、heartbeat fault samples=500、sensor fault samples=1,550、明示reset/recovery=7。正常復帰には明示STARTを要求した。
12. **reason件数**: decoderの正常BINで `safety_reason_mismatches=0`。GNSS_INVALID/IMU_INVALID/IMU_STALE/TOF_INVALID、STOP、E_STOP、HEARTBEAT_TIMEOUTを分離。非有限値はNONFINITEへ分類するコードを追加した。
13. **4出力統計**: left `[-1,1]` non-neutral=72,998 changes=70,618 safe=17,000、right `[-1,1]` 72,991/70,641/17,000、rear `[-1,1]` 72,999/19/17,000、propulsion `[0,0.4]` 73,000/22/17,000。range/slew/nonfinite違反は全て0。
14. **INA/VESC**: INA normal=89,250、invalid=250、missing=250、frozen=250。VESC normal=89,000、invalid=250、missing=250、frozen=250、fault=250。正常BINのfuture join=0、effective-valid違反=0。missing/stale/faultは注入窓として記録される。
15. **独立negative fixture期待値/実測値**: future join INA/VESC=1/1、stale INA/VESC=1/1、invalid INA/VESC=1/1、VESC fault=1、transport gap/reversal/version/unknown/payload/Waypoint CRC/statusは各1。Python全19テストPASS。
16. **Waypoint C++ cases**: BOOT/DISARMED/ARMED_IDLE/RUNNING/E_STOP/FAULTの6状態、count 0/1/16/>16、座標範囲・NaN/Inf、radius 0/負/NaN/Inf、revision duplicate/reverse/reject atomicity、CRC、ACK wire=16を確認。DISARMEDのみAccepted。
17. **Python件数**: `python -m unittest discover -s pc-tools/boat_eskf/tests -q` は19 tests PASS。`compileall`もPASS。
18. **BIN SHA-256**: `min_shadow_30min_a.BIN` と `min_shadow_30min_b.BIN` は同一で、SHA-256は `DA09CA479592E67E1E6D0A35EF2DAB2720A337A21E60C11DB1A2AA77EF879893`。
19. **PlatformIO**: `proposal_shadow_min` SUCCESS（RAM 208,804 / Flash 587,229）、`proposal_shadow_comm` SUCCESS（RAM 190,968 / Flash 896,989）、`m5stack-cores3` SUCCESS（RAM 168,156 / Flash 1,034,473）。
20. **安全フラグ**: `SHADOW_CONTROL_ENABLE=1`、`ACTUATOR_OUTPUT_ENABLE=0`。firmware propulsion range=0..0、INA226実取得無効、PCA/VESC physical write無効。host fixtureのpropulsion=0.4は試験用入力に限る。
21. **未実施操作**: COM3/COM4/COM6接続、書込み、実機センサ試験、SDカード操作、モータ・PCA・VESC出力、main push/mergeは行っていない。新規C++実行ファイルはWindows Defenderの誤検知で再実行が阻止されたため、最終ソースは厳格コンパイルと既生成BIN復号で確認した。
22. **残存制約**: 実機30分連続記録、実microSD/UART負荷、実センサ周期、実物理出力、実機BIN/TXT完全復号は未確認。実機試験は別承認・対象機確認後に行う。
23. **報告パス**: 本書、`docs/PR18_FINAL_AUDIT_20260804.md`、`docs/MIN_SHADOW_OPERATION_SPEC_20260804.md`、`docs/WORK_PLAN.md`、`docs/WORK_LOG.md`、`詳細な報告.md`。
24. **PR URL**: https://github.com/temesotejam/proglam/pull/18

## 判定

ホスト診断・独立負例・C++境界試験・3環境ビルドは是正条件を満たす。実機合格や物理出力有効化を意味しない。次回はDraft PRレビューを継続し、実機試験を行う場合もCOM3を対象外として、別途明示承認を得る。
