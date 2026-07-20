# GNSS往復統合（DRY_RUN）

通信側XIAOがGNSSを取得し、`GNSS_NAV`を10 Hzで制御側へ送ります。制御側はpayload CRC、連番、fixの有効性を検証し、原点からの北／東位置を計算して`GNSS_PROCESS_RESULT`を10 Hzで返します。通信側は送信・返信の両方をSDへ保存し、Web UIと`GET /api/link`で状態を表示します。

通信側のSoftAPは `XIAO-BOAT-TELEMETRY`、パスワードは `12345678`、URLは `http://192.168.4.1/` です。画面更新は4 Hzです。GNSS、SD、UARTの取得はHTTP処理とは独立しています。

共通UARTは921600 bps・8N1です。GNDを共通化し、通信側D6(TX)→制御側D6(RX)、通信側D7(RX)←制御側D7(TX)として接続します。

制御側は `kDryRunActuators=true` が既定です。この間はGNSS計算・結果返信・状態遷移だけを行い、PCA9685のサーボパルスおよび非ゼロVESC dutyは出力しません。安全停止時はPCA9685全OFFとVESC duty 0のみを許可します。

両側は100 msのHeartbeatを送信します。制御側は500 ms以上通信側Heartbeatを受け取れなければ出力を安全状態にしFAULTへ遷移します。通信側はSTOP/E-STOPをcommand ID付きで送り、制御側は`COMMAND_ACK`を返します。時刻同期は1 Hzでrequest/replyを行い、通信側でRTTを記録します。

SDは `/BOATLOG/RUNxxxx.BIN` に全フレームを保存します。通常の「記録停止」では同じ番号の`.TXT`概要も保存します。既存の成功済みロガーは変更していません。

実機確認では、まず二台の新ファームウェアを書込み、USBシリアルで両側のリンク状態を確認します。その後WebのStart logで、GNSS_NAV TXとresult RXがともに約10 Hz、RTT、CRCエラー、連番ギャップ、SD書込みエラーを確認します。STOP/E-STOPはアクチュエータを未接続のDRY_RUN状態で確認します。
