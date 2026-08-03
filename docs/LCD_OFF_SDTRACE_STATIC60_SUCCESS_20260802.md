# LCD停止・SDTRACE 60秒静止試験（RUN0005）

実施日: 2026-08-02

## 変更内容

CoreS3のmain loopにある描画スケジューリングだけを次のように変更した。

```cpp
if(millis()-lastDrawMs>=200){
  lastDrawMs=millis();
  if(!logging&&!logFinalizing)draw();
}
```

`M5.update()`は無条件で継続する。ログ開始前とfinalize終了後は従来どおり描画し、`logging`または`logFinalizing`中は`draw()`を呼ばない。UART、Web API、SDTRACE、ログ形式、SD SPI 10 MHz、SDバッファ、mutexは変更していない。

## 実施手順

- LCD停止版をCOM6へROM no-stubで書込み、全イメージのhash verifiedを確認。
- USB再列挙後、DTR/RTS無効でCOM6へ接続。アプリ診断3行を受信し、生シリアル保存が実際に追記されることを確認した。
- 接続を維持したまま `POST /api/log/start?duration_s=60` を送信。HTTP 202、`{"logging":true,"duration_s":60,"bno_capture":false}`。
- 手動停止は送らず、停止後10秒を超える診断まで連続保存した。
- finalize完了を確認してから、Web APIでRUN0005を回収した。

## 結果

| 判定項目 | 結果 |
| --- | --- |
| RUN番号 | RUN0005 |
| 全512 B write | `write_calls=3060`、全て `actual=512`、失敗なし |
| SDエラー/CMDエラー | なし |
| queue drop | 0 |
| 自動停止 | 成功。60秒のログ後にwriter finalizeへ移行。手動停止なし。 |
| 終了キュー | `q=0` |
| flush/close/TXT | SDTRACEで全て成功 |
| BIN | 1,566,502 B、全11,585レコードを先頭から末尾まで連続復号可能 |
| TXT | 257 B、`records=11585`、`queue_drops=0`、`sd_write_errors=0` |

SDTRACE ringは128件で、今回の連続記録では古い開始イベントが上書きされ、終了時点の128行が出力されている。これは診断ringの設計どおりであり、`write_calls=3060`と全記録のBIN復号で持続書込みの完走を検証した。

## 判定と今後

この条件でRUN0004の途中SD失敗が再現せず、RUN0005が60秒を完全に完走した。従って**CoreS3のLCD更新とmicroSDの共有SPI競合/負荷を有力な原因候補として記録する**。

- CoreS3専用の複雑なSPI排他実装は追加しない。
- CoreS3では画面をログ中停止する現行方針を暫定採用する（将来必要なら1 Hz以下表示を別途評価）。
- 本来のBNO・ToF・GNSS試験へ進める。
- XIAO移行後には、XIAO上のSD連続記録を改めて検証する。

保存物は `pc-tools/boat_eskf/captures/SDTRACE_LCD_OFF_20260802_*` にある。
