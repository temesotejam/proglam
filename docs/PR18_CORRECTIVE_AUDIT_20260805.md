# PR #18 最終ホスト監査 是正報告（2026-08-05）

対象は `feat/proposal-benchmark-replay-20260804` と PR #18 です。本書は UTF-8 で再生成した現行の監査報告であり、旧報告中の文字化け・旧SHA・旧テスト件数・空transport診断を現行値として扱いません。

1. 作業開始HEAD: `2fd66f6604686fe54b8891d4f35c55df57a19589`
2. 試験対象コードHEAD: `ae2084efc0998c7cc74a23c59ed781f884e79902`
3. 最終文書・証跡・push HEAD: 文書commit後の完全SHAをPR #18本文と最終コメントへ記録する。
4. PR状態: Draft / Open / Unmerged を維持する。
5. mainとの差: base main は `f4e2908366ea139c8d55d2de71f4e2595be25438`。mainへのpush・mergeはしていない。
6. 今回のコミット: `ae2084e audit: complete PR18 host evidence checks`。
7. 主な変更: cycle manifest、state/reason生成freshness、正式transport経路、Waypoint本番handler試験、全byte BIN golden、freeze/CSV/report証跡。
8. 6未達点の是正: 文字化けは本書でUTF-8再生成、reasonはmanifest照合、negativeはCRC/COBS/slew完全一致、Waypointは本番経路、goldenは38 byte完全比較、証跡はZIPまで保存した。

## safety scenario と reason 照合

9. 独立scenario: STOP、E_STOP、HEARTBEAT_TIMEOUT、GNSS_INVALID、GNSS_STALE、IMU_INVALID、IMU_STALE、TOF_INVALID、TOF_STALE、NONFINITEを各1 cycleで発生させ、各後続cycleで `reset()` と明示STARTを行った。
10. expected reason件数: NONE=89,990、STOP=1、E_STOP=1、HEARTBEAT_TIMEOUT=1、GNSS_INVALID=1、GNSS_STALE=1、IMU_INVALID=1、IMU_STALE=1、TOF_INVALID=1、TOF_STALE=1、NONFINITE=1。
11. actual reason件数: expectedと完全一致。
12. mismatch: `safety_reason_mismatches=0`、`safety_reason_mismatches_by_expected_reason={}`、`first_safety_reason_mismatch=null`。
13. manifestなし解析: `safety_reason_expectation_status=not_provided`、`safety_reason_mismatches=null`。mismatch=0とは表示しない。
14. 独立negative fixture: INA/VESC future・missing・stale・invalid・fault、sequence gap/reversal、version、unknown、payload length、nonfinite output、state、reason未提供、range、safe output、slew、STOP restart、Type66/67 CRC/status。
15. fixture非0値: 各fixtureは対象値のみ完全一致。slew fixtureは `slew_violations=1`、range/safe/nonfinite/state/STOP restartは0。
16. 対象外カウンタ: `test_min_shadow_independent_negative.py` に `assertGreater` / `assertGreaterEqual` は残していない。
17. CRC-only: 正式 `boat::encode → boat::Decoder` で `crcErrors=1,cobsErrors=0,lengthErrors=0,handler=0,ACK=0`。
18. COBS-only: 正式Decoderで `cobsErrors=1,crcErrors=0,lengthErrors=0,handler=0,ACK=0`。

## Waypoint

19. apply試験: count 0/1/16/17、緯度/経度の下限上限・範囲外・NaN・Inf、radius 0/負/NaN/Inf、action、revision増加/同一/逆行、6受付状態を独立配列で検査した。
20. ACK検査: 正常Type66および内容拒否はhandler=1/ACK=1。Type67、16 bytes、request ID、revision、status、reason、active index、count、canonical CRC、transport CRCをdecodeまで確認した。
21. atomicity: request ID、revision、count、active index、reach radius、16点の緯度経度をfieldwise比較。Duplicate/Rejectedで不変。handlerはController参照を持たない。
22. malformed length: 0/1/4/7/8/sizeof-1/sizeof/sizeof+1。4 bytes未満はrequest ID=0、4 bytes以上でID取得、8 bytes未満はrevision=0、8 bytes以上でrevision取得。範囲外読取りなし。

## serializer と freeze

23. BIN golden期待38 bytes: `47 4F 4C 42 2E 16 00 00 00 00 00 00 01 43 04 00 09 00 00 00 07 00 00 00 D2 04 00 00 00 00 00 00 AA 55 10 20 30 40`。
24. BIN golden実測: `written=38`、全38 byte一致、容量1 byte不足・header/payload長不一致・payload上限超過は失敗し `written=0`。
25. stale閾値: GNSS 500,000 us、IMU 100,000 us、ToF 250,000 us、INA/VESC 500,000 us。sensor stale初回はそれぞれ520,000 / 120,000 / 260,000 usをscenarioで通過した。
26. INA freeze: fixed timestamp=299,980,000 us、freeze=250、timestamp変化=0、age単調違反=0、first stale=520,000 us、stale=225/225、recovery timestamp=305,000,000 us、recovery failure=0。
27. VESC freeze: fixed timestamp=719,980,000 us、freeze=250、timestamp変化=0、age単調違反=0、first stale=520,000 us、stale=225/225、recovery timestamp=725,000,000 us、recovery failure=0。

## 最終実行

28. C++環境: MinGW g++、`-std=c++17 -Wall -Wextra -Werror`。
29. C++ build: `g++ -std=c++17 -Wall -Wextra -Werror -Ipc-tools/boat_eskf/cpp_tests -Ishared/proposal_min/src -Ishared/bin_record_serializer/src -Ixiao-boat-control-integration/lib/boat_protocol/src ...`。
30. host binary SHA-256: `974E0A574D1832BAA3430EF15D7D7FF212EB76EC665B1E1EB1B7BC7AD27A5BFA`。
31. 30分試験コマンド: `pr18_final_host.exe <BIN> <manifest.json> <transport.json>` をA/B各1回。Waypoint hostとgolden hostも同HEADで実行。
32. モデル時間: 1,800 s、周期20 ms（50 Hz）、Controller呼出し90,000回。wall time A=5.497762 s、B=5.411078 s。
33. Type件数A/B: Type63=90,000、64=89,750、65=89,750、66=1、67=1、合計269,502。
34. BIN SHA-256 A/B: 両方 `8E4F0D0B510A8A2690AA1BFF453B9851AA8DA09B627EA910426C6845040DB7EF`。
35. CSV行数 A/B: 90,000 / 90,000。
36. transport診断 A/B: `decoded_frames=269502,crc_errors=0,cobs_errors=0,length_errors=0`。
37. Python: CPython 3.9、unittest 39 tests PASS、`compileall` PASS、state/reason generated freshness PASS。
38. PlatformIO build: proposal_shadow_min SUCCESS (RAM 208,804 / Flash 587,505)、proposal_shadow_comm SUCCESS (RAM 190,968 / Flash 897,393)、m5stack-cores3 SUCCESS (RAM 168,156 / Flash 1,034,693)。uploadはしていない。
39. 安全flag: `SHADOW_CONTROL_ENABLE=1`、`ACTUATOR_OUTPUT_ENABLE=0`。実機propulsion=0..0、INA226実取得なし、PCA9685物理出力なし、VESC制御送信なし、AS5600不使用。
40. 実施していない操作: COM/USB探索、COM3/4/6接続、upload、microSD、実機センサ、サーボ/翼/後部ヨー/推進、Defender変更、main push/merge、rebase/squash/force push、Draft解除。
41. 証跡: `pc-tools/boat_eskf/captures/PR18_FINAL_HOST_AUDIT_20260805.zip`。内部にBIN A/B、CSV A/B、JSON report A/B、manifest A/B、transport A/B、command/decode log、SHA256SUMS、wall time、host binaryを保存。ZIPは固定timestampで作成し、展開後SHA-256を検証した。
42. 残件: 実機30分、実microSD/UART、実センサ周期、物理Waypoint ACK、実機BIN/TXT復号は未実施。
43. 更新監査報告: `docs/PR18_CORRECTIVE_AUDIT_20260805.md`。
44. PR URL: https://github.com/temesotejam/proglam/pull/18

ホスト監査の完了は実機安全確認の完了を意味しない。次の明示指示があるまで実機へ書き込まない。