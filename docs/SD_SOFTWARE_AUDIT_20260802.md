# CoreS3 SDアクセス差分・整合性監査

作成日: 2026-08-02  
対象: `m5stack-cores3-telemetry-bridge`

## 監査の結論

現行版に、payloadポインタの寿命切れ、payload長の境界超過、BINレコード長の不整合、またはwriterタスク自身がclose後にwriteする経路は見つからなかった。一方で、SDアクセスを`SdWriter`だけに一元化できてはいない。ログ開始・ファイル一覧・ダウンロードはメインループ（Web/タッチ起点）でSD APIを呼び、グローバルの`File logFile`は開始側からwriter側へ所有権が渡る構造である。

これだけでCMD18/CMD0D/CMD00を直接説明する証拠はまだない。しかし、以前の成功実績版には共通`logMutex`とコマンド遅延実行があり、現行版の`sdMutex`はwriterタスクしか取得していない。この差は診断対象として残る。

添付指示に従い、現行版のログ形式・UART・ESKF・制御機能を変えず、SDアクセスの時系列診断を追加してCoreS3へ書込み済みである。次の別microSDカード試験では、操作の呼出元・時刻・write要求/実書込み量・flush・closeまでシリアルに`SDTRACE`として出力される。

## 比較対象の特定

ワークスペース内で確認できる、UART→SD保存の実機成功根拠は過去の通信側XIAO版`xiao-boat-telemetry-integration`である。`RUN0007`は23,660レコード、1,884,004 bytes、保存区間121.377秒で、制御側シーケンスの欠番0が記録されている。

「138秒以上成功した版」という指定と完全一致するソース/ログは現在のワークスペースにはなかったため、この121.377秒の保存実績版を比較基準にした。CoreS3の保存済み`rev2`は25 MHz/4 KB writerスタック版で、成功版としては扱わない。

## 構成差分

| 観点 | 過去の通信側XIAO成功実績版 | CoreS3現行診断版 |
| --- | --- | --- |
| 起動 | `Serial.begin()` | `M5.begin()`後にSPI/SD初期化 |
| SPI/SD初期化 | `SPI.begin(...)`後、既定SDクロック | `SPI.begin(GPIO36/35/37/4)`後、明示的に10 MHz |
| ログキュー | `boat::Frame[160]` | `boat::Frame[96]` |
| `boat::Frame`実寸 | 816 bytes | 816 bytes |
| キュー領域 | 130,560 bytes | 78,336 bytes |
| SDステージング | 8,192 bytes、512 byte単位書込み | 同じ |
| writer | `LogWriter`、4 KB、優先度2、Core1 | `SdWriter`、12 KB、優先度1、Core1 |
| 排他 | 開始・writer・停止が同じ`logMutex`を使用 | `sdMutex`はwriterだけが取得。開始/Webは未取得 |
| Web開始/停止 | Webは`pendingCommand`へ要求を入れ、後で処理 | Webハンドラ/タッチから`startLog()`/`stopLog()`を直接実行 |
| 終了 | admissionを閉じ、排他下でdrain→flush→close→TXT | writerが非同期finalizeでdrain→flush→close→TXT |
| 診断 | SD書込み回数、要求/実書込み量、最大書込み時間をTXTへ記録 | 今回、同等の時系列`SDTRACE`をシリアルへ追加 |

CoreS3では以前4 KBスタックで`SdWriter`のstack canary panicが発生したため、12 KB化は必要な修正である。今回のCMD18/CMD0D/CMD00は12 KB化後にも発生しており、スタック不足とは別問題である。

## SDアクセスの全経路（現行診断版）

| 呼出元 | 実行コンテキスト | SD操作 | ログ中の可否 |
| --- | --- | --- | --- |
| `setup()` | Arduinoメイン初期化 | `SPI.begin`、`SD.begin`、`mkdir` | ログ前のみ |
| `startLog()` | メインループ。HTTP `POST /api/log/start`またはタッチ | `mkdir`、`exists`、BINの`open` | ログ開始直前 |
| `commit()` | Core1の`SdWriter` | `logFile.write` | BINログ中 |
| `logTask()`のfinalize | Core1の`SdWriter` | 最終write、`flush`、`close` | 停止時 |
| `writeSummary()` | Core1の`SdWriter`から呼出し | TXTの`open`、`printf`、`close` | BIN close後 |
| `apiLogFiles()` | メインループWebハンドラ | ディレクトリ`open`、`openNextFile`、`close` | `logging`/`logFinalizing`なら409で拒否 |
| `apiLogDownload()` | メインループWebハンドラ | file `open`、`streamFile`、`close` | `logging`/`logFinalizing`なら409で拒否 |

従って「SDアクセスは`SdWriter`だけか」という問いの答えは**いいえ**である。ただしBINへの連続`write`、最終`flush`、`close`は`SdWriter`だけが行う。

## File所有権とclose/writeの検査

`logFile`はグローバルである。`startLog()`がメインループ側でBINをopenし、`logging=true`後にCore1の`SdWriter`がwrite/flush/closeする。厳密には複数タスクにまたがる所有権移管である。

通常経路では、openは`logging=true`より前、writerのwriteは`logging`中、flush/closeは同じwriterのfinalize部だけで実行される。close後に同じwriterがwriteへ戻る分岐は存在しない。停止要求とwriter処理の競合時も、停止側はフラグを変えるだけでcloseを実行しないため、writerが途中の最大24フレームを書いた後に次周回でfinalizeする可能性はあるが、closeと同時にwriteする経路はない。

ただし`sdMutex`を取得するのはwriterだけである。Web/API側のSD操作を相互排他する用途にはなっていない。現在のWeb APIは記録中/最終化中を409で拒否し、同一メインループで順次処理されるため通常は重ならないが、構造上の一元化は未達である。

## データ本体・境界・BINレコードの検査

| 項目 | 実装 | 判定 |
| --- | --- | --- |
| UARTデコードpayload | `header.length <= 768`とraw総長/CRCを検証後にcopy | 境界検査あり |
| ログキュー投入 | `enqueue(boat::Frame frame)`は値渡し。`queue[qHead]=frame`で全体コピー | 一時変数へのポインタは渡さない |
| ローカルフレーム | `emitLocal()`は長さを768以下に制限し、payloadへcopy後に値渡し | 境界検査あり |
| `Frame` | packed header 22 + payload 768 + 時刻3個。アライン込み816 bytes | 固定長 |
| BINレコード | magic 4 + ingress時刻8 + header22 + payload長0〜768 | 34〜802 bytes |
| SDバッファ | 8,192 bytes。`append()`は空き容量までcopyし、満杯時にcommit | 境界超過なし |
| SD転送 | 512 bytesずつ。要求値と`File.write()`戻り値を比較 | 不一致は失敗扱い |
| `snprintf`/`memcpy` | RUN名16 bytes、path32 bytes。`/BOATLOG/RUN9999.BIN`でも収まる | 監査範囲で超過なし |
| GNSS raw | 最大110 bytesへclampして110 byte配列へcopy | 境界超過なし |

TXT要約はBINのclose後にwriterから作る。`apiLogFiles`/`apiLogDownload`は記録中または最終化中に409を返すため、BIN記録中にTXT作成・Web読出しが動く通常経路はない。

なお、現行`writeSummary()`はSD書込み失敗後でも`normal_stop=1`を固定で書く。このTXTが作成できた場合に異常停止を正常停止と誤認し得るため、診断上の欠点である。ただし今回のCMD18時はTXT open自体が失敗している。

## 新しい時系列診断

追加した固定長`SdTraceEvent[128]`は、SDアクセス元、操作、時刻、記録件数、キュー件数、write要求/実書込み量、成功フラグを保持する。512 byte writeは最初の3回、以後16回ごと、または不一致時に記録する。終了後に`SDTRACE`としてシリアルへ出力するため、計測中の書込み負荷をほぼ増やさない。

追加RAMは3,104 bytes（ビルド値145,452→148,556 bytes）。これはグローバル固定配列であり、`SdWriter`スタックを消費しない。flashは1,015,721→1,016,985 bytesである。

## 次試験

別のmicroSDカードをFAT32でフォーマットして挿入後、診断版のまま静止60秒ログを行う。比較条件はSD SPI 10 MHz、UART 921600 bps、ESKF/センサ/ログ形式は現状のままとする。

判定時は以下を確認する。

1. `SDTRACE`で`log_start`のmkdir/exists/openが成功していること。
2. writerの512 byte writeで`requested=actual=512`が継続すること。
3. 失敗時は、最後の成功writeと不一致write、flush/close/TXT openの順を確認すること。
4. 60秒後にwriterのflush→close→summary open/write/closeが一度ずつ出ること。
5. CMD18/CMD0D/CMD00の有無、records、queue drops、BIN/TXT取得を確認すること。

