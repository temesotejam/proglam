# SDTRACE付き静止60秒ログ再試験（RUN0004）

実施日: 2026-08-02

## 条件固定

microSD、SD SPIクロック（10 MHz）、SDバッファ、mutex、BIN/TXT形式、CoreS3ファームウェアは変更していない。CoreS3の再起動も行わず、稼働中のCOM6へDTR/RTS無効で接続した。

## 診断値のコード上の意味

| 値 | 正確な意味 |
| --- | --- |
| `rec` | `logTask`（SdWriter）が1フレームをBINレコードへ直列化し、`append()`が成功してから増やす数。生成数・キュー投入数ではない。staging bufferに残る未flush分を含み得るため、常に物理SD書込み完了数でもない。 |
| `q` | `Frame`リングキューの占有フレーム数（byte数ではない）。`q=3`等は未処理フレームがその数だけ残った意味。 |
| `final` | `logFinalizing`。`stopLog()`で1になり、writerがcommit/flush/close/TXT/SDTRACEを処理した後に0へ戻す。診断は1秒周期のため、短いfinalizeでは`final=1`行を必ずしも捕捉しない。 |
| `log` | `logging`。自動停止/手動停止の`stopLog()`、またはwriter内の`append()`失敗で0になる。 |

自動停止はmain loopが期限到来を検出して`stopLog()`を呼び、`logFinalizeRequested=true`としてwriterへ通知する。writerの`logTask`がmutexを取得してcommit、flush、close、TXT要約生成、SDTRACE出力を行う。

## シリアル事前確認

- USBリセットはしていない。
- COM6へ接続後、アプリ診断を3行受信してから生ログ保存を開始・維持した。
- 保存ファイルは準備確認時点で420 byte、その5秒後に1,955 byteへ増加し、アプリ診断が実際に追記されることを確認した。
- この接続を保持したまま開始要求を送った。PowerShell HTTPクライアントの開始応答取得はプロキシ経由例外となったが、同じ保存シリアルで`log=1`と記録増加を確認したため、CoreS3は開始要求を受理している。

## 試験結果

- RUN番号: `RUN0004`。
- 開始要求時刻（PC記録）: 2026-08-02T04:58:35.0058927Z。開始応答そのものはPCプロキシ例外で未取得。
- `log=1`、`rec=20`を最初に確認。その後 `rec=2822` までは継続した。
- SDTRACEの最初のwriteは `us=694047929`、512 B要求/512 B実書込みで成功。
- 769回の512 B write成功後、770回目で `us=710406124`、`rec=2880`、`q=5`、512 B要求に対して0 Bとなった。
- 同時刻にSDライブラリは `ff_sd_status(): Check status failed` を3回出力。今回の捕捉範囲にはCMD18/CMD0D/CMD00行はない。
- 失敗後、同一writerタスクのSDTRACEはflush成功、close成功、TXT open/write/close成功の順である。TXTには `records=2880`、`sd_write_errors=1`、`queue_drops=0` が保存された。
- 失敗後10秒を超えて診断を継続し、`log=0`、`final=0`、`rec=2880`、`q=5` が継続した。未処理5フレームが残ったままfinalizeが完了している。
- 60秒自動停止時刻は存在しない。約16.78秒後のSD書込み失敗により早期終了した。停止APIは送っていない。

## 判定

ユーザー指定の分岐 **B（write成功後に途中で失敗）**。初期化や最初のFile open/writeではなく、持続書込み中にSDの状態確認が失敗した。現行コードでは失敗時に`logging=false`とfinalize要求を設定し、キュー全件を排出する前にflush/close/TXTへ進む経路も観測された（`q=5`残留）。この試験では条件変更や修正は行っていない。

## 保存物

`pc-tools/boat_eskf/captures/SDTRACE_STATIC60_RETRY_20260802_*` に、生シリアル、復元表示用ログ、SDTRACE全行、SDライブラリエラー全行、診断時系列、Web応答、RUN0004 BIN/TXT、ハッシュ一覧を保存する。
