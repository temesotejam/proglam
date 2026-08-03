# BNO08X 静止60秒試験（不成立）

実施日時: 2026-08-02 JST

## 条件

- CoreS3: RUN0005で成功したLCD停止条件を維持（`logging` または `logFinalizing` 中は `draw()` を呼ばず、`M5.update()` は継続）。SD SPI 10 MHz。
- XIAO: BOAT_EXPERIMENT=24。加速度・ジャイロ 100 Hz、磁気 20 Hzに加え、Game Rotation VectorとLinear Accelerationを各50 Hzで有効化したビルドをCOM4へ書込み（ハッシュ検証済み）。
- `shadow_only=true`、`actuator_output_enabled=false` を開始前の `/api/eskf` で確認。
- 同一microSD、UART・ESKF・ログ形式・バッファ・mutex・SD設定は変更しない。

## 実施結果

1. CoreS3をリセットせずCOM6へ接続し、アプリ診断3行を受信後、生シリアル保存を開始した。
2. 保存ファイルが増加することを確認後、`POST /api/log/start?duration_s=60&bno_capture=1` を送信。HTTP 202、`{"logging":true,"duration_s":60,"bno_capture":true}` を保存した。
3. 自動停止を待ったが、開始後にCoreS3のアプリUSBシリアル出力が停止し、Web APIおよびICMPも無応答となった。70秒を超えたため停止APIを送る条件だったが、Web API自体に接続できず送信不能だった。
4. ESP-ROMの書込みツールではCOM6/MAC `30:ed:a0:d4:bf:40` を認識する一方、アプリ側のUSBシリアル読み出しは `ClearCommError / WinError 22`、SoftAPはSSID接続状態のまま通信不能だった。
5. フラッシュ書込みなしのhard reset、および試験前に確認済みの同一CoreS3ファームウェアのCOM6明示復旧書込み（全hash verified）後も、アプリWeb APIは復帰しなかった。

## 判定

この試行は静止データ試験として不成立である。実測時間、BIN/TXT、BNO各軸統計、ESKFログ、SDTRACE終端は取得できていないため、正常データとして扱わない。

この障害はRUN0005との差分であるBNO追加50 Hz報告を有効にした直後に発生したが、途中BIN/TXTとSDTRACE終端を回収できていない。そのため、原因をBNO負荷、CoreS3、SD、またはカード状態のいずれかと断定しない。

## 保存済み証跡

`pc-tools/boat_eskf/captures/BNO_STATIC60_20260802/` に以下を保存した。

- 開始前ESKF応答と開始HTTPヘッダ・本文
- 生シリアル、シリアル準備/終了状態
- Web APIタイムアウト、ping失敗のコマンド結果
- ROM再起動および同一ファームウェア復旧書込みの結果

次の試験へ進む前に、CoreS3のアプリ起動を物理的なUSB抜き差しまたは電源再投入で回復させ、`/api/eskf` とアプリ診断3行を再確認する必要がある。
