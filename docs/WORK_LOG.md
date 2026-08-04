# 作業ログ

## 2026-08-01 — BNO08X→ESKF入力の修正と実機確認

- 添付指示に従い、UART配線・921600 bps/8N1・COBS+CRC・Wi-Fi設定・メッセージ型・SDログ形式・ESKFの数式/状態/ノイズ/取付/GNSS遅延・物理出力設定は変更しなかった。
- RUN0001を全15,377レコードから再解析した。ログ時刻の実録時間は92.853619秒。Type 24は1,788件で、BNO生タイムスタンプはAccel/gyro/magで逆行・重複を含んだ。
- 原因: `bnoEvent()` がジャイロの `sensorUs`（SH-2生タイムスタンプ）を `shadowEskf.predict()` に渡していた。`Shadow::predict()` は単調増加を要求するため、逆行値を拒否してIMU ageが未受信値のままになった。
- 修正: ESKFの予測時刻のみ、単調なMCU受信時刻 `rxUs` を使用するよう変更。`sensorUs`はPrimaryImuSnapshotのログ値として保持し、UARTプロトコルを変えていない。BNO処理数、再初期化数、受信年齢、ESKF候補/対/予測/受理/理由別拒否をシリアル診断へ追加。
- `bno_accel100_gyro100_mag20_int_60sec` をビルド（RAM 111,076 / 327,680、Flash 557,925 / 3,342,336）し、COM4、MAC 34:85:18:ab:fa:90へ書込み。全領域のハッシュ照合成功。
- 実機診断: BNOは0x4A、init/reinit=1/0。最終読出し時にAccel/Gyro/Mag=30,612/24,213/6,054、ESKF candidate/pair/call/accepted=24,213/24,213/24,213/24,213、全拒否理由=0。ALIGNINGは2,003,559 us・201サンプルで完了。
- Core `/api/eskf`でRUNNING(2)、health=Degraded(1; mount_valid=0)、IMU年齢4.2 ms、time_reversals=0、imu_gaps=0、有限値・共分散有効、shadow_only=true、actuator_output_enabled=falseを確認。
- RUN0002をSDへ保存しPCへ回収。TXT: normal_stop=1、records=12,317、queue_drops=0、sd_write_errors=0。開始前後でUART CRC/COBS/lengthカウンタは0/15/17のままで、試験中増加は0。
- RUN0002の実録時間は74.233763秒。ESKF State 1,432件は全件RUNNING・Degraded、IMU年齢は平均3,576.6 us、最小252 us、最大12,566 us、reset=0、shadow_only=1、actuator_output=0。Health 368件は全件finite/covariance valid、最終imu_gaps=0、time_reversals=0。
- 静止中にもNED位置・速度が大きくドリフトした。今回は入力時刻の根本原因だけを修正する範囲のため、ESKF数式・ノイズ・取付設定は変更していない。
## 2026-08-01 — health理由・BNOイベント時刻診断の追加

- 添付指示に従い、ESKF数式、状態次元、ノイズ、BNO取付値、GNSS/ToF観測、UART、Wi-Fi、SDバイナリ形式、メッセージpayload、出力無効設定を変更しなかった。予測時刻は引き続きXIAO受信時刻`rxUs`である。
- `eskf.cpp`のhealth判定を確認。ALIGNING完了時に`kEskfMountValid=false`なら`publicHealth_=Degraded`、trueならValidとなる。`state()`/`health()`はIMU staleまたは非有限値をInvalidにする。GNSS/ToF未使用、bias値、不確かさ、観測timeoutはDegradedへ遷移させる条件ではない。今回のDegraded直接理由はmount未検証だけ。
- CoreS3 `/api/eskf` とWeb UIへ `health_reason` と `health_diagnostic_mask` を追加。実機は `health_reason=mount_unvalidated`、mask=129（mount未検証=1、GNSS未受信の観測状態=128）。判定条件は変更していない。
- BNO Readerへ、Accel/Gyro/Magイベント別に`uint64_t`・µsのSH-2 `sensorUs`と単調なXIAO `rxUs`を集計する診断を追加。unique数はPSRAM上の8192スロット集合で正確に集計し、連続重複、逆行、rollover、signed sensor dt、rx dt、対応する最初/最後の時刻対、生値の軸別平均・標準偏差・min/max・norm・非有限値数をUSBシリアルへ出力する。
- 実機起動確認ではAccel/Gyro/Magの`rxUs`平均dtは約7.91/10.00/40.01 ms、`sensorUs`は逆行が多数、rollover=0、unique集合overflow=0。BNO生加速度平均は約[-5.953,-1.188,-7.849] m/s²で、現時点のidentity入力（ESKF入力と同じ）ではFRD水平静止の期待[0,0,-9.80665] m/s²と不一致。軸試験で取付を確定するまで変換値を変更しない。
- CoreS3の既存ログAPIへ`duration_s=1..600`を追加。開始時刻から指定時間で自動停止する。バイナリログレコード形式と通信payloadは不変。
- XIAO環境を再ビルド（RAM 111,724 / 327,680、Flash 561,681 / 3,342,336）しCOM4へ書込み。CoreS3環境を再ビルド（RAM 145,444 / 327,680、Flash 1,015,245 / 6,553,600）し、通常スタブ失敗後にROM no-stub方式でCOM6へ書込み、全イメージのhash照合に成功。
- 次は、機体を水平な固定台へ置いて触れないことを確認後、XIAOの`imu_stats_reset`とCoreの`POST /api/log/start?duration_s=60`でRUN0003以降の正確な静止ログを取得する。
## 2026-08-01 — 静止試験のログ基盤診断

- 失敗したP1生BNOキャプチャがXIAO側で有効のまま残り、Coreへ約500 frame/sを送っていた。BOAT_EXPERIMENT=24の同一XIAOファームウェアをCOM4へ再書込み（Flash 561,745 bytes、RAM 111,724 bytes、全イメージhash verified）してP1状態を解除した。
- Coreの10秒ログ診断で、`SdWriter`（4KB stack）にStack canary watchpointを確認した。SD CMD18/CMD0D失敗の後にCore 1 panicし、RUN0004--RUN0010は0 byteとなった。原因はSD/FAT処理を含む書込みタスクのスタック不足である。
- Coreを修正: `SdWriter` stack=12KB、優先度=1、反復ごとに1ms譲渡。終了要求・flush・close・TXT要約をSD書込みタスクに一元化し、記録中のSD一覧/ダウンロードを409で拒否した。Core build成功（RAM 145,452/327,680、Flash 1,015,721/6,553,600）、COM6 ROM no-stub書込み・全hash verified。
- 修正後RUN0011は10秒ログとして208,556 byteとTXTを正常保存し、再起動直後の直接ダウンロードでPCへ回収できた。HTTP Serverは最初の要求後に不安定化するため、本60秒測定はCore再起動直後の最初のHTTP要求で開始し、終了後にCoreを再起動してSDから回収する必要がある。現時点で60秒本測定は未開始。- RUN0012: Core再起動直後の最初のHTTP要求で`duration_s=60`開始応答を確認し、60秒待機後に再起動して回収したが、BINは0 byteでTXTなし。静止データとして無効。60秒記録中のCoreシリアル監視が次の必須診断である。- 60秒監視ログ（RUN0013）で本因を確定: 開始後`rec=63`で `[sd_diskio.cpp] sdCommand(): Card Failed! cmd: 0x0d`、続いてCMD00が連続失敗し、`fopen(/sd/BOATLOG/RUN0013.TXT) failed`。以後`logging=0, rec=63, q=3`のままCoreメインループ/UARTは継続。従って現在の0 byteログ原因はmicroSDのSPIカード応答喪失（接触、電源、配線、または25 MHz信号品質）であり、ESKF・UARTプロトコル・SD書込みタスクのスタックではない。- SDカード再装着後のRUN0014（10秒診断）でも`rec=512`まで書込み後に`CMD18`失敗、続いてCMD0D/CMD00およびTXT fopen失敗を再現。従ってカード再装着では解消せず、SPI信号品質・給電・カード自体のいずれかが原因。現在の25MHz SDクロックは未変更。
## 2026-08-01 — SD SPI 10 MHz 切り分け（書き込み待ち）

- ユーザー承認により、CoreS3のSD初期化だけを SD.begin(kSdCsPin, SPI, 25000000) から 10000000 へ変更した。UART、ESKF、センサ設定、ログ形式は変更していない。
- CoreS3ファームウェアのビルドは成功（RAM 145,452 / 327,680 bytes、Flash 1,015,721 / 6,553,600 bytes）。
- ROM no-stub方式でCOM6への書き込みを試行したが、Windowsのシリアルポート一覧にCOM6が存在せず、COM4（制御側）だけを検出したため、未書き込み・未検証。再接続後に10秒ログでCMD18/CMD0D/CMD00エラーの有無を確認する。

## 2026-08-01 — SD SPI 10 MHz 検証結果

- 10 MHz版をCoreS3（COM6、MAC 30:ed:a0:d4:bf:40）へROM no-stub方式で書き込み、全イメージのhash verifiedを確認した。
- 再起動後、SoftAP BOAT-ESKF-CORE で最初のHTTP操作として POST /api/log/start?duration_s=10 を実行し、HTTP 202を確認した。
- シリアル診断ではログ中に 
ec=65 の後、sdCommand(): Card Failed! cmd: 0x18、続いてCMD0D/CMD00、open(/sd/BOATLOG/RUN0016.TXT) failed を確認した。10 MHzへ下げても25 MHz時と同じSD SPI失敗であり、クロック速度だけが原因ではない。
- UART・ESKF・センサ設定・ログ形式は変更していない。次の切り分け対象はmicroSDカード、カードソケット接点、3.3 V電源、およびSD SPI配線/信号品質である。

## 2026-08-01 — 作業中断時点の保存

- 次回の引継ぎ用に、ソース・設定・docs・PC解析ツール（ビルド生成物を除く）を oat-current-program-handoff-20260801-rev3-sd10mhz.zip としてワークスペース直下へ保存した。
- 次回は docs/PROJECT_CONTEXT.md、docs/WORK_PLAN.md、docs/WORK_LOG.md の順に確認し、SDの次の切り分けをmicroSDカード交換から再開する。CoreS3にはSD SPI 10 MHz版が書き込み済みである。

## 2026-08-02 — フォーマット済みmicroSDの再検証

- ユーザーがmicroSDをフォーマットしてCoreS3へ挿入。CoreS3を標準リセットしてSDを再初期化し、Wi-Fi 2を BOAT-ESKF-CORE へ接続した。
- POST /api/log/start?duration_s=10 はHTTP 202。SD SPI 10 MHz設定でログを実行し、シリアル診断で 
ec=81 → 334 → … → 1719 を確認後、log=0, q=0, 
ec=1719 で自動停止した。CMD18/CMD0D/CMD00、SD write failed、panicはいずれも出ていない。
- したがってフォーマット後の10秒ログは暫定合格。前回の失敗にはカードのファイルシステム状態が関与した可能性がある。ただしWeb APIは起動直後の最初のリクエスト以外がタイムアウトする既知の問題のため、ログ一覧/TXTをHTTPでは読めず、次回の60秒静止試験で持続性とPCへのBIN/TXT取得を確認する。

## 2026-08-02 — フォーマット後の静止60秒ログ（失敗）

- 静止準備完了後、CoreS3を再起動してSDを再初期化。Wi-Fi 2を BOAT-ESKF-CORE へ接続し、起動後最初のHTTP操作として POST /api/log/start?duration_s=60 を実行した（HTTP 202）。
- ログ開始直後は 
ec=157, 
ec=342 まで正常に増加したが、
ec=411 で sdCommand(): Card Failed! cmd: 0x18 が発生。その後CMD0D/CMD00、open(/sd/BOATLOG/RUN0002.TXT) failed を確認した。ログは60秒を完走せず log=0, q=0, rec=411 で停止。
- 前回の10秒ログ（1,719レコード）は一時的成功に過ぎず、フォーマットだけでは持続書込みを保証できないことが確定。10 MHz化も有効な恒久対策ではない。次の最優先は別microSDカードでの同一60秒試験である。

## 2026-08-02 — 詳細報告書

- 現状、実測時系列、原因の切り分け、次手順を docs/SD_LOGGING_STATUS_REPORT_20260802.md にまとめた。

## 2026-08-02 — SDソフトウェア差分監査と時系列診断

- 添付指示に従い、過去の通信側XIAO成功実績（RUN0007、121.377秒、23,660レコード、欠番0）とCoreS3現行を比較した。完全な138秒版はワークスペースに見つからなかったため、確認可能な最長成功実績を基準にした。
- 監査結果: 現行はpayloadを値コピーしており、payload長768 byteの検査、BINレコード34..802 byte、8,192 byte staging bufferと512 byte chunkの境界は整合している。writer自身のclose後write経路はない。一方、SDアクセスはwriterだけには限定されず、開始/Web一覧/Webダウンロードもメインループ側でSD APIを使い、logFileの所有権は開始側からwriter側へ移る。
- SdTraceEvent[128]による呼出元・時刻・open/write/flush/close時系列診断を追加。ログ形式、UART、ESKF、制御出力は変更していない。CoreS3ビルド成功（RAM 148,556/327,680、Flash 1,016,985/6,553,600）後、COM6へROM no-stub書込み・全hash verifiedを確認した。詳細は docs/SD_SOFTWARE_AUDIT_20260802.md。

## 2026-08-02 — 現行microSD・SDTRACE 60秒静止ログ（1回のみ）

- ユーザー指定に従い、現在挿入中のmicroSDとSDTRACE追加済みCoreS3ファームウェアのまま、`POST /api/log/start?duration_s=60` を1回だけ送信した。開始は13:44:41 JST、HTTP 202、応答は `{"logging":true,"duration_s":60,"bno_capture":false}`。試験中にSDカード、SDクロック（10 MHz）、バッファ、mutex、Web処理、BIN/TXT形式を変更していない。
- 終了後に `POST /api/log/stop`（HTTP 200、`{"logging":false}`）とファイル一覧を取得。`RUN0003.BIN=0 byte`、`RUN0003.TXT`は不在で、BINは0 byteとしてPCへ回収、TXT取得はHTTP 404だった。
- アプリ診断の捕捉部分には `log=0, q=3, rec=7971, final=0` が残った。これは60秒相当の記録カウンタ到達を示すが、開始〜停止のシリアルが欠落したため自動停止完了・SD書込み完了を証明するものではない。
- USBリセット後の生シリアル取得がROM起動ログだけを受信し、USB再列挙後に取得器を切り替えた時点ではSDTRACEダンプ区間を過ぎていた。したがって、保存済みのSDTRACE行は0、CMD18/CMD0D/CMD00等のSDライブラリCMD行も0である。これはCMDエラーなしの証拠ではない。TXT不存在確認に伴う `vfs_api.cpp` のopen失敗1行はあるが、SD CMDエラーではない。
- CoreS3表示画面はPCから取得する経路がなく、失敗時画面の画像は未取得。試験後に再試験やファームウェア変更は行っていない。取得完全性、保存物、判定は `docs/SDTRACE_STATIC60_TEST_20260802.md`、証跡は `pc-tools/boat_eskf/captures/SDTRACE_STATIC60_20260802_*` に固定保存。
## 2026-08-02 — SDTRACE付き60秒静止ログ再試験（RUN0004）

- ユーザー指定どおり、microSD、SD SPIクロック10 MHz、SDバッファ、mutex、ログ形式、CoreS3ファームウェアを変更せず、CoreS3を再起動せずに試験を1回実施した。COM6へDTR/RTS無効で接続し、アプリ診断3行と生ログ追記を確認してから接続を維持したまま開始要求を送った。
- 開始HTTP応答はPCのプロキシ経由例外で取得できなかったが、同一の生シリアルに`log=1`・`rec=20`以降の増加があるため、CoreS3が開始要求を受理したことを確認した。RUN番号はRUN0004。
- SDTRACE: 最初の512 byte writeは`us=694047929`で成功。769回成功後、`us=710406124`、`rec=2880`、`q=5`で770回目が0 byteとなった。`ff_sd_status(): Check status failed`を3回捕捉した。CMD18/CMD0D/CMD00行は今回の捕捉範囲にはない。
- 失敗後、writerタスクがflush（成功）→close（成功）→TXT open/write/close（成功）を実施し、SDTRACEを出力した。TXTは257 byteで`records=2880`、`sd_write_errors=1`、`queue_drops=0`。なお現行TXTの`normal_stop=1`は失敗時にも固定文字列で出力する実装であり、正常停止の証拠には使えない。
- 60秒自動停止には到達せず、約16.78秒後の持続書込み失敗で早期終了。停止APIは送っていない。失敗後10秒超の診断は`log=0, final=0, rec=2880, q=5`で継続し、未処理5フレームを残してfinalizeが終了した。
- `--noproxy '*'`でfinalize後のWeb回収を実施し、ファイル一覧、RUN0004.BIN（0 byte）、RUN0004.TXT（257 byte）を保存した。完全な生シリアル、SDTRACE全行、SDライブラリエラー全行、時系列、HTTP応答、ハッシュは`pc-tools/boat_eskf/captures/SDTRACE_STATIC60_RETRY_20260802_*`に保存。詳細は`docs/SDTRACE_STATIC60_RETRY_REPORT_20260802.md`。
## 2026-08-02 — LCD描画停止版の準備

- ユーザー指定どおり、`loop()`の描画スケジューリングだけを `if(!logging&&!logFinalizing)draw();` に変更した。ログ開始前とfinalize終了後は従来どおり約200 ms周期でdrawし、logging中とfinalize中のみdrawを呼ばない。`M5.update()`は無条件で維持した。
- CoreS3ビルドは成功（RAM 148,556 / 327,680、Flash 1,017,013 / 6,553,600）。COM6へROM no-stub書込みし、bootloader/partition/boot_app0/firmwareの各hash verifiedを確認。
- 書込み後のhard resetでCOM6が再列挙されず、Windows上はCOM4（制御XIAO）のみ。DTR/RTS無効のアプリシリアル取得器は起動できず、指定された診断3行確認および60秒試験は開始していない。接続復帰待ち。
## 2026-08-02 — LCD停止・SDTRACE 60秒静止試験（RUN0005、合格）

- 指定どおり、`logging`または`logFinalizing`中だけ`draw()`を呼ばない変更をCoreS3へ実装。`M5.update()`は維持し、UART、Web API、SDTRACE、ログ形式、SD SPI 10 MHz、SDバッファ、mutexは不変。ビルド成功（RAM 148,556 / 327,680、Flash 1,017,013 / 6,553,600）、COM6へROM no-stub書込み、全hash verified。
- USB再列挙後、DTR/RTS無効のCOM6接続でアプリ診断3行と生ログ追記を確認。接続を維持して`POST /api/log/start?duration_s=60`を実行し、HTTP 202 `{"logging":true,"duration_s":60,"bno_capture":false}`を保存。手動停止は送らなかった。
- RUN0005は60秒後に自動停止。SDTRACE `write_calls=3060` は全て512/512 byte成功で、SD/CMDエラー0。終了時`q=0`、TXTはrecords=11,585、queue_drops=0、sd_write_errors=0。flush/close/TXT open/write/closeもSDTRACEで成功。
- RUN0005.BINは1,566,502 byte。既存binlog形式で先頭から末尾まで全11,585レコードを連続復号し、消費byte数はファイルサイズと一致。不正magic・途中切れ・再同期はない。停止後10秒超も`log=0, q=0, rec=11585, final=0`を確認。
- LCD更新停止でRUN0004の途中SD失敗が再現せず完走したため、CoreS3 LCD-SD共有SPI競合/表示負荷を有力候補として記録。複雑なSPI排他は追加せず、CoreS3の画面はログ中停止を暫定方針とする。詳細は`docs/LCD_OFF_SDTRACE_STATIC60_SUCCESS_20260802.md`、証跡は`pc-tools/boat_eskf/captures/SDTRACE_LCD_OFF_20260802_*`。
## 2026-08-02 — BNO08X静止60秒試験（不成立）

- XIAO BOAT_EXPERIMENT=24にBNO08X Game Rotation Vector 50 HzおよびLinear Acceleration 50 Hzを追加し、既存の加速度・ジャイロ100 Hz、磁気20 Hzを維持した。COM4へ明示指定で書込み、全hash verified。
- CoreS3はRUN0005のLCD停止条件、SD SPI 10 MHz、同一microSDのまま。開始前`/api/eskf`で`shadow_only=true`と`actuator_output_enabled=false`を確認。USBリセットなしで診断3行後に生シリアル保存を開始した。
- `POST /api/log/start?duration_s=60&bno_capture=1`はHTTP 202。開始後、CoreS3のアプリUSBシリアル、Web API、ICMPが無応答になった。70秒超時点で停止APIを送るべき状態だったが、APIに接続不能で送信できなかった。BIN/TXT、SDTRACE終端、BNO統計は回収不能。
- COM6のROM書込みツールはMAC `30:ed:a0:d4:bf:40`を認識。hard reset、さらに試験前に確認済みの同一CoreS3ファームウェアのCOM6明示復旧書込み（全hash verified）を行ったが、アプリ通信は復帰しなかった。
- この試行は不成立であり、RUN0005の成功・再現性には数えない。証跡と詳細は`docs/BNO_STATIC60_ATTEMPT_20260802.md`および`pc-tools/boat_eskf/captures/BNO_STATIC60_20260802/`に保存。

## 2026-08-02 — CoreS3単体30秒健全性確認

- ユーザー操作でCoreS3/XIAOを完全電源断、XIAO TX→CoreS3 RXを取り外し、CoreS3 USBを10秒抜いた後にCoreS3のみを起動。
- USBリセットなしのCOM6生シリアルで診断3行以上を受信。全診断で`bytes=0, frames=0, log=0, q=0, rec=0, final=0`を確認し、ログ開始は行わなかった。
- Wi-Fi 2は`BOAT-ESKF-CORE`へ接続。pingは開始前3/3成功（2–3 ms）、32秒後3/3成功（1 ms）。`/api/eskf`は開始前・32秒後ともHTTP応答し、XIAO未接続のため`state_received=false`、`xiao_link.connected=false`であることは期待どおり。
- CoreS3単体は少なくとも32秒正常。保存証跡: `pc-tools/boat_eskf/captures/CORE_STANDALONE_20260802/`。追加BNOデータ送信下の障害は未分離のため、次はログを開始せずUART接続のみを戻して同一監視を行う。

## 2026-08-02 — CoreS3 + XIAO UART受信のみ32秒監視

- ユーザー操作でCoreS3電源断後にXIAO TX→CoreS3 RXのみを復帰。ログを開始せず、COM6で診断3行以上を保存。
- `/api/eskf`は開始前・32秒後とも応答。`shadow_only=true`、`actuator_output_enabled=false`、`xiao_link.connected=true`、ESKF state/health/baseline受信を確認。pingは開始前・32秒後とも3/3成功（1–2 ms）。CoreS3停止は再現しなかった。
- 32秒間にCore UART受信は1,443,878→2,021,382 byte、10,538→14,750 frame。CRC/COBS errorは0/0である一方、sequence_gapsは2,754→2,774、length_errors=1を観測。これは今回のCore停止ではないが、ログとの組合せ試験前に調査対象とする。
- 保存証跡: `pc-tools/boat_eskf/captures/CORE_UART_NLOG_20260802/`。ログ開始は行っていない。

## 2026-08-02 — 追加BNO報告、raw BNO記録なし10秒ログ（RUN0007成功）

- 全UART配線を接続したまま、`POST /api/log/start?duration_s=10&bno_capture=0`を送信。HTTP 202、`bno_capture=false`。手動停止なし。
- RUN0007: BIN 259,122 byte、TXT 257 byte。TXTは`normal_stop=1`、records=1,918、queue_drops=0、queue_high_water=10、sd_write_errors=0、control_crc/cobs/length=0/0/1。
- 生シリアルでlogging中のrec=98→1,816、q=0..6を確認。自動停止後は`log=0,q=0,rec=1918,final=0`。SDTRACEのwrite_calls=507は全512/512成功、flush/close、TXT open/write/closeもすべてok=1。
- BINは既存復号器で1,918件、259,122/259,122 byte消費、trailing=0。queue timestamp span=9,983,638 us。保存証跡: `pc-tools/boat_eskf/captures/BNO_EXTRA_NOCAP_LOG10_20260802/`。

## 2026-08-02 — raw BNO記録有効10秒ログ（障害再現）

- `POST /api/log/start?duration_s=10&bno_capture=1`はHTTP 202。開始直後からCOM6生シリアルは停止し、10秒後に期待される`log=0`・flush・close・TXT生成の診断は出なかった。
- 開始後のpingは3/3成功（1 ms）でWi-Fi/ICMPは生存したが、`/api/eskf`と`/api/log/files`は各10秒でHTTP応答なし（curl exit 28）。このため自動停止・finalize・BIN/TXT回収は不可能で、手動停止も送信できなかった。
- 再現比較: 同じ追加BNO報告を維持し`bno_capture=0`にしたRUN0007は10秒・1,918件・SDエラー0で正常完走。`bno_capture=1`だけで障害が再現した。
- コード確認: Core `serviceControl()`は`while(controlUart.available())`で無制限にデコード、cache、enqueueを繰り返し、byte/frame予算がない。一方XIAO受信側には`kLinkRxByteBudget`がある。raw BNOでUARTが継続非空になるとCore main loopがWeb/自動停止/診断へ戻れない仮説と、ICMPだけ生存する観測が一致する。未修正のため原因は「最有力候補」として記録する。
- 保存証跡: `pc-tools/boat_eskf/captures/BNO_EXTRA_CAP_LOG10_20260802/`。

## 2026-08-02 — 詳細報告の更新

- ユーザー指示により、ルートの `詳細な報告.md` を過去版から今回のCoreS3 BNO生データ記録停止の詳細報告へ置換した。
- 報告には確定済み試験、RUN0007正常結果、`bno_capture=1` での再現結果、最有力仮説、未確定事項、次の最小修正案、および保存済み証跡を記載した。
- ファームウェア、配線、microSD、SDクロック、バッファ、mutex、ログ形式、ESKF、安全設定には変更なし。

## 2026-08-02 — Core UART受信公平化修正・10秒再試験

- 添付指示の修正前確認を実施。Core UART受信はArduino loopTask内（ESP32-S3 frameworkのCore 1、priority 1）、`while(controlUart.available())`で無制限だった。SdWriterはCore 1、priority 1、stack 12 KiB。RX bufferは16 KiB。`enqueue()`は満杯時dropして即時returnする非ブロッキング処理。
- Core `serviceControl()`に最大2048 byte、32 decoded frame、2000 usのいずれかで戻る予算を導入。decoder状態、CRC/COBS/length判定、payload、UART、SD、ログ形式、BNO周期、mutexは変更していない。RXDIAGへbytes/s、frames/s、BNO kind/s、処理時間、budget hits、RX buffer最大、queue、loop、heap、reset reasonを追加。
- ビルド成功: RAM 148,668 / 327,680、Flash 1,018,865 / 6,553,600。PlatformIO stub uploadはflash verification失敗のため、ROM no-stubでCOM6へbootloader/partition/boot_app0/firmwareを書込み、全hash verified。
- COM6再列挙後、COM6/CoreとCOM4/XIAOをDTR/RTS無効で同時接続。診断3行を受信してから`POST /api/log/start?duration_s=10&bno_capture=1`を送信。開始応答はHTTP 202、`bno_capture=true`。
- 10秒試験はCoreアプリ停止を再現せず、ping/API応答、診断継続、自動停止、q=0、flush/close/TXT、BIN復号が成功。RUN0009: 470 records、BIN 62,046 bytes、queue_drop=0、SD write error=0、512B write traceは全ok。
- UARTは開始前からエラーが存在し、終了時CRC/COBS/length=3/77/78。RXDIAGのtime budget hit=13,655、RX buffer max=16,384、decoded frameはログ開始後32件に留まり、BNO期待周期を満たさなかった。10秒合格条件は不合格、60秒静止試験は未実施。
- XIAO USB診断はBNO addr 0x4A、nonfinite=0、link queue drop=0/high-water=14を示したが、XIAO専用`bnoLinkFrameDrops`はUSB診断へ出ていないためBNO送信drop=0は未確定。安全状態はFAULT/dry=1でアクチュエータ出力無効。
- 保存証跡: `pc-tools/boat_eskf/captures/BNO_FAIR_RX_BUDGET_LOG10_20260802_RERUN/`。詳細は`詳細な報告.md`。

## 2026-08-02：BNO UART分離10秒診断 RUN0011

- Core COM6とXIAO COM4を再接続後に識別。Core USBシリアルは115200 bpsでアプリ診断を取得し、開始前Core 15行/XIAO 80行を保存。
- curlで開始APIを1回送信。HTTP 202、bno_capture=true、bno_log_enqueue=false。
- 対象RUN0011のBINは64,539 byte、489件、trailing byte=0。TXT records=489、normal_stop=1、sd_write_errors=0、queue_drops=0。
- SDTRACE上のRUN0011 writer writeはreq=512/actual=512、flush/close/TXT open/write/closeは全てok=1。
- XIAOはBNO kind 1/2/4を630/500/250件、drop=0、partial=0、zero=0で送信。kind 3/5は0件。
- CoreのTRIAL_ENDはrx_bytes=4,998、frames=45、bno_valid=0、bno_kind=0/0/0/0/0、trial CRC/COBS/length=0/0/0、rxbuf_full=12。finalize後の累積COBS/length/sequenceは増加。
- 分岐Bと判定。ログqueue/SDではなくCore UART受信処理・decoder・RX buffer飽和を優先調査し、60秒試験を保留。
- 証拠保存先: pc-tools/boat_eskf/captures/BNO_UART_DECOUPLED_ENQ0_LOG10_20260802_RERUN4/。詳細: docs/BNO_UART_DECOUPLED_ENQ0_RUN0011_20260802.md。
## 2026-08-02 — Core UART専用受信タスク診断・A/B/C

- RUN0011のXIAO encoded 263,584 byte / 10.002 sを再計算し、8N1物理占有率を28.6%へ訂正した。2.9%は誤り。
- 変更前CoreはArduino loopTask（Core1/framework priority1）内の`while(controlUart.available())`でUARTを無制限処理していた。SdWriterはCore1/priority1/stack12KiB、UART RX bufferは16KiB。
- Core `controlRxTask`をCore1/priority2/stack8192で追加。1回最大512 byte、UART timeout2 ms、1 tick yield、UART reader/decoderを一元化。loopTask、Web、M5.update、SD設定、mutex、ログ形式、BNO設定は維持。
- `raw`、`decode_count`、`dispatch_no_bno_enqueue`、`dispatch`モードと、UART B/s、read、delimiter、decoded frame、BNO kind、RX max/full、stack HWM、loop max、SDTASK max process、heap診断を実装。
- XIAOに型別requested/enqueued/completed/encodedBytesを追加。P1開始でリセットし、停止時に型別スナップショットを出力。
- A RUN0012 raw: 258,560 byte取り込み、frames=0（raw仕様）、RX full=0、decoder error=0、BIN457件/末尾0、SD/TXT成功。
- B RUN0013 decode_count: frames=2,633、BNO kind=630/500/0/250/0、CRC/COBS/length=0/0/0、RX full=0、BIN442件/末尾0、SD/TXT成功。
- C RUN0014 dispatch_no_bno_enqueue: frames=2,631、BNO kind=631/500/0/250/0、queue drop=0、SD/TXT/BIN成功。現行診断版RUN0015もframes=2,633、BNO kind=630/500/0/250/0、`SDTASK max_process_us=106813`、queue drop/SD error=0、BIN1711件/末尾0で完走。
- kind3（Game Rotation Vector）とkind5（Linear Acceleration）は全試験0件。受信タスクやSDの障害とは分離して、XIAO設定・送信経路の調査を先行する。
- ビルド成功（RAM150,500/327,680、Flash1,023,569/6,553,600）。COM6 ROM no-stub書込み、全イメージhash verified。書込み後アプリ診断3行以上と新`SDTASK`行を確認。
- 保存先: `pc-tools/boat_eskf/captures/UART_RX_RAW_LOG10_20260802/`、`UART_RX_DECODE_COUNT_LOG10_20260802/`、`UART_RX_DISPATCH_NO_BNO_ENQ_LOG10_20260802/`、`UART_RX_DISPATCH_CURRENT_LOG10_20260802/`、`CORE_RXTASK_UPLOAD_VERIFY_20260802/`。詳細は`docs/UART_RX_TASK_DIAGNOSTIC_REPORT_20260802.md`。
## 2026-08-02 — Core型別encoded byte診断追加（書込み待ち）

- 添付必須項目の「Core message type別byte数」を満たすため、COBS delimiterを含む受信フレーム長を`TRIALTYPE ... encoded_bytes`として型別集計する実装を追加した。`TypeStat`と試験baselineへ型別byte累計を追加し、UART/SD/ログ形式は変更していない。
- ビルド成功: RAM154,596/327,680、Flash1,023,745/6,553,600。
- 自動C試験終了後にCoreS3 COM6がWindowsへ再列挙されず、最新版は未書込み。COM4（XIAO）のみ検出。CoreS3再接続後、ROM no-stub書込み・起動確認・B/C再取得を行うまで、この追加診断を実機合格扱いにしない。
## 2026-08-02 — 型別encoded byte版の書込みと最新B/C

- COM6復帰後、Core型別encoded byte診断版（RAM154,596、Flash1,023,745）をROM no-stub書込み。bootloader/partitions/boot_app0/firmware全hash verified。書込み後8秒診断を`CORE_RXTASK_TYPEBYTES_UPLOAD_VERIFY_20260802/`へ保存。
- RUN0016（decode_count）: HTTP202、trial rx=253,572 byte/2,640 frames、CRC/COBS/length=0/0/0、BNO kind=630/500/0/250/0、型別encoded bytes合計253,572、`SDTASK max_process_us=128970`、queue drop/SD error=0、BIN465件/末尾0、TXT/flush/close成功。
- RUN0017（dispatch_no_bno_enqueue）: HTTP202、trial rx=253,000 byte/2,637 frames、CRC/COBS/length=0/0/0、BNO kind=632/500/0/250/0、`SDTASK max_process_us=76144`、queue drop/SD error=0、BIN1734件/末尾0、TXT/flush/close成功。TRIALTYPE合計は終了出力境界で72 byte（1フレーム）先行するため、TRIAL_ENDを正本とした。
- B/CともXIAO kind3/Game Rotation Vectorとkind5/Linear Accelerationは0件。BNO設定・送信経路の別問題として保留。
- 開始API timeoutでログを開始できなかったB/C試行は不成立としてcaptureへ保存し、合格数へ含めない。
- 詳細報告追記: `docs/UART_RX_TASK_DIAGNOSTIC_REPORT_20260802.md`。
## 2026-08-02 BNO個別10秒試験・イベント経路計測

- XIAOへkind3-only、kind5-only、全5reportの試験設定を追加し、Core側は既存のdedicated UART RX task、SD、ログ形式、UART設定を維持した。
- CoreS3 COM6へ最新ファームウェア、XIAO COM4へ全5report試験ファームウェアをビルド・書込みし、ハッシュ確認済み。
- RUN0024: kind3イベント506、TX完了506、Core受信506、effective 50.013 Hz。sequence gap=6のため不合格。
- RUN0025: kind5イベント506、TX完了506、Core受信506、effective 50.020 Hz、sequence gap=0。SD write/flush/close/TXT/BINも成功し合格。
- RUN0026: kind1/2/3/4/5のイベント、TX完了、Core受信は638/506/506/253/506で一致、drop=0。ただしsequence gap=14、kind1=63.062 Hz、kind4=25.006 Hzで不合格。
- 60秒静止試験は、ユーザー指定の合格条件未達のため実施していない。
## 2026-08-02 — sequence境界診断・BNO個別試験の終了時点

- CoreS3へ、START_ACK/STOP_ACK、同一sequence閉区間、SEQUENCE_GAP履歴、Core BNO到着時刻統計を追加した診断版を書込み。ビルドRAM 158,508/327,680、Flash 1,026,785/6,553,600。ROM no-stub書込み、全hash verified。
- XIAOへK3、ALL、K1、K4を順に書込み、各10秒自動試験を実施。RUN0027–RUN0030として保存。
- K3はgap=2（67030/67031）、ALLはgap=26、K1はgap=6、K4はgap=2。XIAO側TX drop/partial/zero=0、Core CRC/COBS/length=0、RX full=0。K3/K4の代表欠落はXIAO TX_HISTORY flags=0x07でC候補。
- K1 Core到着Δt=10.265/15.869/33.039 ms（631件）、K4=47.075/49.975/53.863 ms（200件）。設定値はK1 20,000 us、K4 50,000 us。ALLではK1/K4実効Hzも要求値から外れた。
- `bno_log_enqueue=0` のため今回BINからBNO raw sensor timestampを再計算できない。K1/K4のBNO_TSは起動後累積metricsとして保存されており、試験区間専用値ではない。明日baseline付き統計を追加する。
- 本日はユーザー都合によりここで停止。dispatch試験および60秒静止試験は開始していない。詳細引き継ぎは`docs/SEQUENCE_DIAGNOSTIC_HANDOFF_20260802.md`。
## 2026-08-03 — XIAO送信順序保証、K4再試験（RUN0032合格）

- `BNO_TS_TRIAL`をXIAOへ追加。P1開始時にkind 1–5の試験用timestamp統計をゼロ化し、停止時にevents、重複、逆行、sensor/RXのmin/mean/max、最初/最後を出力する。K4ではtrial=200件、sensor dt=-200/0.00/300 us、RX dt=48,526/49,972.80/51,496 usを取得。
- RUN0031はCore trial seq_gap=2だったが、XIAO TX_HISTORYでsequence 8157/8158はいずれもflags=0x07（allocated/enqueued/completed）。8158が8157より先にenqueueされたため、Coreが8158→8157→8159を受信した順不同と確定した。
- XIAO `linkSend()`は従来、linkMuxをsequence採番後に解放してencode/queue投入していた。BNO taskとloop taskの競合でFIFO順が逆転するため、採番からencode・FIFO投入までを同一linkMux区間に変更。UART速度、BNO設定、プロトコル、payload、SD、ログ形式、アクチュエータ設定は不変。
- K4修正後RUN0032: HTTP 202、P1 boundary first/last=1971/3371、Core trial frames=1412、BNO kind4=200、trial seq_gap=0、CRC/COBS/length=0/0/0、RX full=0。XIAO TX event/req/enq/done=200/200/200/200、drop=0。Core BNO到着=200、45,967/49,969.98/53,827 us。
- RUN0032はrecords=596、BIN=81,827 bytesを596 records・trailing=0で全件復号。queue drop=0、SD error=0、q=0、flush/close/TXT成功。証跡: `pc-tools/boat_eskf/captures/BNO_K4_SEQORDER_TRIAL_20260803/`。
- 書込み対象はMAC照合済みXIAO COM4（34:85:18:AB:FA:90）のみ。別用途CoreS3 COM3（44:1B:F6:E2:09:F8）には一切接続・書込みしていない。CoreS3 COM6は30:ED:A0:D4:BF:40を照合した。
- ROM書込み時にQIOを指定するとbootloaderヘッダが生成物のDIO（E9 03 02 3F）からQIO（E9 03 00 3F）へ書換わり、TG0WDT_SYS_RSTループとなることを確認。DIOで再書込みし正常起動を確認した。以後COM4 XIAOのROM書込みにはDIOを使用する。

## 2026-08-03 — K3/ALL送信順序修正後の10秒再試験

- K3環境をDIOでCOM4（MAC 34:85:18:AB:FA:90）へ書込み、hash verified。RUN0033はtrial seq_gap=0、kind3=500、XIAO TX event/req/enq/done=500/500/500/500、Core到着15,389/19,993.76/24,082 us。trial CRC/COBS/length=0/0/0、RX full=0、queue drop/SD error=0、flush/close/TXT成功。RUN0033.BIN=77,723 byteは568 records・trailing=0で復号。
- ALL 5 reports環境を同じくDIOでCOM4へ書込み、hash verified。RUN0034はtrial boundary=1446..5190、trial seq_gap=0、Core BNO kind=631/500/500/250/500。XIAO TX完了も631/500/500/250/500で一致、drop/partial/zero=0。trial CRC/COBS/length=0/0/0、RX full=0。
- RUN0034はSD error=0、queue drop=0、q=0、flush/close/TXT成功。RUN0034.BIN=77,480 byteは566 records・trailing=0で全件復号。
- ただしBNO_CAPTURE_REPORTの実効Hzはkind1=63.118（設定50）、kind2=50.017、kind3=50.016、kind4=25.029（設定20）、kind5=50.016。従ってALLの周期合格条件は未達であり、dispatch enqueueおよび60秒静止試験を実施していない。
- 保存証跡: `pc-tools/boat_eskf/captures/BNO_K3_SEQORDER_TRIAL_20260803/`、`pc-tools/boat_eskf/captures/BNO_ALL_SEQORDER_TRIAL_20260803/`。

## 2026-08-03 — RUN0035 direct-connection retry (ALL 5 BNO reports)

- Safety identification: only boat XIAO COM4 (MAC 34:85:18:AB:FA:90) and boat CoreS3 COM6 (MAC 30:ED:A0:D4:BF:40) were used. COM3 was not present and was never opened.
- Root cause of the initial retry failure was the PC HTTP proxy: curl had sent http://192.168.4.1/... through the university Squid proxy. The harness must set NO_PROXY=* / no_proxy=* (or use curl --noproxy "*") for every CoreS3 SoftAP API call.
- Current CoreS3 source was built successfully. Ordinary PlatformIO upload reached the stub but failed before data transfer (Unable to verify flash chip connection); ROM --no-stub upload to COM6 then wrote 0x10000 successfully and verified the data hash. No XIAO write was performed.
- Direct API check returned HTTP 200 with shadow_only=true, actuator_output_enabled=false, and XIAO link sequence_gaps=0.
- POST /api/log/start?duration_s=10&bno_capture=1&bno_log_enqueue=0&uart_rx_diag=decode_count returned HTTP 202. RUN0035 completed automatically.
- Core trial: bno_kind=632/500/500/250/500; seq_gap=0; CRC/COBS/length=0/0/0; RX buffer full=0; sequence boundary 58132..61875 with matching start/stop ACK boundaries.
- XIAO transmit counts matched Core exactly: kind1/2/3/4/5=632/500/500/250/500, with request/enqueue/done all equal and drops=0.
- SD: 512-byte writes all ok=1; final queue=0; flush/close/TXT open/write/close all ok=1. TXT: records=573, queue_drops=0, sd_write_errors=0. BIN: 78,350 bytes, 573 decoded records, trailing bytes=0, record sequences and queue timestamps monotonic.
- Rate acceptance remains unresolved and reproducible: BNO kind1 reports about 63.2 Hz although 50 Hz was requested, and kind4 about 25.0 Hz although 20 Hz was requested. Kinds 2/3/5 are about 50 Hz. Do not enable raw-BNO log enqueue or start the 60-second static test until the accepted report-period behaviour is diagnosed and any condition change is approved.
- Evidence: pc-tools/boat_eskf/captures/BNO_ALL_RETRY_DIRECT_20260803/.
## 2026-08-03 — BNO Feature Response診断の実装・ビルド

- ユーザー添付の「BNO08X受理周期と最終要求周期の確定」に従い、現在のBOAT_EXPERIMENT=20（K1/K2/K3/K4/K5=50/50/50/20/50 Hz要求）を変更せず、SH-2 Get Feature Response（0xFC）診断を追加した。
- `bno_reader`は各`enableReport()`成功直後、全report設定後、`wasReset()`後の再設定、P1 capture開始要求後に、BNO専用タスクからのみ`sh2_getSensorConfig()`を実行する。出力にはreport ID、requested/accepted interval・Hz、flags、change sensitivity、batch interval、sensor-specific、要求/応答時刻、statusを含める。
- `P1Capture`開始は既存のBNO試験統計リセット後にFeature Responseスナップショットを要求するだけであり、UART、SD、ログ形式、ESKF、アクチュエータ出力条件は不変。
- `platformio run -e bno_all_5reports_10s`成功: RAM 205,284/327,680、Flash 572,409/3,342,336。実機書込み・Feature Response取得は未実施。
## 2026-08-03 — BNO Feature Response実機診断 RUN0036–RUN0039

- BOAT_EXPERIMENT=20の要求周期を変更せず、Get Feature Response診断をCOM4 XIAO（MAC 34:85:18:AB:FA:90）のみへDIO書込みした。COM3は未検出・未接続・未書込み。
- 初版はFeature ResponseをPipelineMetricsへ4世代×5種保持し、RUN0036で既存loopTaskのstack canaryを発生させた。backtraceはGNSS処理中で、metrics増量がスタック余裕を消費したと判断。診断値を生シリアルのみへ最小化し、RUN0036は不成立扱い。
- RUN0037 ALL: snapshot Feature ResponseはK1=16,000us/62.5Hz、K2=20,000us/50Hz、K3=20,000us/50Hz、K4=40,000us/25Hz、K5=20,000us/50Hz。Core/XIAO送受信数は632/500/501/250/500で一致、seq_gap=0、trial CRC/COBS/length=0/0/0、RX full=0、SD/TXT/BIN正常。
- RUN0038 K1: requested 20,000usに対しaccepted 16,000us、TX/Core=631/631、seq_gap=0、SD/TXT/BIN正常。
- RUN0039 K4: requested/acceptedとも50,000us、TX/Core=200/200、seq_gap=0、SD/TXT/BIN正常。K4=25HzはALL有効化時だけの組合せ依存と確定。
- BNO timestamp fieldは重複/逆行を示し、単位・wrap拡張未確定。時刻delta分布（min/mean/max以上）とCore unknown type=23を最終周期決定前に診断する。最終要求周期は未変更・未承認。
- 最後にCOM4をALL 5種Feature Response診断版へDIO復帰書込みしhash verified。
## 2026-08-03 — timestamp/type23診断と最終要求周期試験

- CoreS3診断をビルドしCOM6 MAC 30:ED:A0:D4:BF:40へ書込み。COM3は未接続・未操作。
- type=23をP1Captureとして復号。RUN0040〜0046でpayload length=16、CRC/COBS/length正常、trial unknown=0。
- BNO timestamp生成元をAdafruit SH-2ソースで確認。`hal_getTimeUs()=millis()*1000`、`touSTimestamp()`がreference/delay補正と32-bit rolloverを合成。生値をBNO独立時刻と解釈しない。
- K1/K4/ALLの診断フォルダに生シリアル、API、BIN/TXT、timestamp_analysisを保存。XIAO/Core arrivalとreport sequenceは単調、欠落0。
- 最終周期環境27〜30を追加。K1 100 Hz、K2 100 Hz、K4 20 Hz、ALL 100/100/20/50/50 Hzをコード固定。4環境ビルド成功。
- COM4 MAC 34:85:18:AB:FA:90を毎回照合してDIO標準配置で書込み。RUN0043〜0046を自動停止まで実施。
- 最終試験は全RUNでHTTP202、queue/drop/RXfull/CRC/COBS/length/SD write error=0、flush/close/TXT成功、BIN trailing=0。ALL磁気のみFeature accepted 25 Hz（要求20 Hz）。
- 詳細: `詳細な報告.md` 末尾、`pc-tools/boat_eskf/captures/BNO_FINAL_*_20260803/`。
## 2026-08-03 RUN0047 最終ALL raw BNO保存確認

- 対象COM4 XIAO / COM6 CoreS3のみをMAC照合。COM3は未操作。
- 指定APIをHTTP 202で実施。要求周期とFeature accepted周期を分離記録（100/100/50/20/50 Hz → 125/100/50/25/50 Hz）。
- XIAO event/TX enqueue/TX complete/Core受信は kind 1..5 = 1261/1000/500/250/499 で一致。
- BIN保存は 1258/997/499/250/498 で段階差分が発生。BIN全件復号・trailing=0、flush/close/TXT成功。
- TRIAL_END transport errors=0、type23 ACK識別=2件。queue最大81、finalize後queue=27残留、SDTASK max_process_us=255027。
- 判定は不合格。通常運用周期への切替は保留し、writer/finalizeのキュー排出条件を次に調査する。証跡は `pc-tools/boat_eskf/captures/BNO_RAW_ENQ_DISPATCH_ALL_RETRY_20260803/`。
## 2026-08-03 RUN0048–RUN0051 finalize競合修正・検証

- RUN0048/0049の診断で、`enqueue_after_writer_exit=7`、`enqueue_after_file_close=7`、finalize後queue残量11〜19を確認。UARTやSD write returnではなく、stop/finalizeとenqueueの状態遷移競合と特定した。
- Core loggerへIDLE/RUNNING/STOP_REQUESTED/CLOSING_INPUT/DRAINING/FINALIZING/FINALIZED/ERROR状態、enqueueゲート、active enqueue数、type/kind別段階カウンタ、pending queue列挙、writer終了条件を追加した。
- COM6へビルド・書込み・hash検証。COM3は未接続・未操作。
- RUN0050: queue=0、全kind段階一致、BIN完全復号。RUN0051: `enqueue_accepted=5346`, `queue_dequeued=5346`, `written_to_bin=5346`, `pending_in_queue=0`, `enqueue_after_writer_exit=0`, `enqueue_after_file_close=0`, FINALIZED。BIN 5,346 records/trailing=0、TXT/flush/close成功、UART/SDエラーおよびdrop=0。
- RUN0051の周期は要求/accepted/実測を分離保存（100/100/50/20/50 Hz要求、125/100/50/25/50 Hz accepted、実測約126.55/100.03/50.04/24.99/50.02 Hz）。
- 証跡: `pc-tools/boat_eskf/captures/BNO_FINALIZE_FIXED_ALL_FINAL_20260803/`、詳細は`詳細な報告.md`末尾。
## 2026-08-03 RUN0052 VESC/PCA9685取り外し60秒静止ESKF試験

- VESCとPCA9685を物理的に取り外し、actuator出力なし・shadow_only=true・dry=1で試験。対象はCOM4 MAC 34:85:18:AB:FA:90、COM6 MAC 30:ED:A0:D4:BF:40のみ。COM3は未操作。
- API HTTP202、自動STOP ACK（start 98662 / first 98663 / stop 125696 / last 125695）、finalize FINALIZEDを確認。
- RUN0052のBNO kind 1/2/3/4/5は7581/6002/3001/1501/3001で、XIAO eventからBINまで段階差0。UART/SDエラー0、queue=0、BIN 2,876,796 bytes・27,724 records・trailing=0、TXT records=27,724。
- ESKF APIスナップショット5/15/30/45/60秒を保存。finite=1、covariance_valid=1、reset_count=0、quaternion normは約1.0。ただし5→60秒で速度・NED位置が大きくドリフトし、predict bad-dt rejectが328増加。innovation APIはreceived=falseで時系列なし。
- 判定: ロガー・通信・保存経路は合格、静止ESKF性能は不合格。通常運用周期・アクチュエータ統合・航行試験へ進まない。証跡は`pc-tools/boat_eskf/captures/BNO_ESKF_STATIC60_20260803/`、解析JSONは`RUN0052_analysis_summary.json`。

## 2026-08-03 RUN0053 ESKF??10???
- ??????predict dt??????????trace?ESKF reset_at_start?innovation/update counters??XIAO COM4?Core COM6?????????COM3?????
- Core??stub???????COM6 MAC 30:ED:A0:D4:BF:40?????ROM no-stub?????hash verified?
- ??API?HTTP 202?????????????????Core?XIAO?????????P1 START/STOP ACK?????????TRIAL_END?105?????raw BNO 0??STOP_ACK timeout?LOGGER_STATE ERROR?
- SDTRACE?512 B write?flush?close?TXT???????????ESKF??????????????RUN0053??? pc-tools/boat_eskf/captures/BNO_ESKF_DIAGNOSTIC10_20260803/ ??????TX/RX/GND????????10??????????
## 2026-08-03 作業再開：RUN0052再解析/RUN0053通信経路追跡

- 実施：RUN0052の既存証跡（BIN/TXT/API/Core-XIAO生ログ/解析JSON）を非破壊で再確認。27,724 records、trailing=0、段階件数差0、UART/SDエラー0を確認。
- 実施：RUN0053のCoreログで ESKF_RESET_AT_START queued=1 を確認。Core試験区間は rx_bytes=0 開始、終了時もBNO kind=0。XIAOログにreset/P1 START/P1 STOP ACKなし。
- 実施：Core GPIO8/9、XIAO D6/D7、921600 baud、8N1、共有packed protocol定義、CRC/COBS実装、dispatch call siteを確認。
- 判断：Core→XIAOの最初の失敗はXIAO linkUart.available()/decoder入口前。CRC/COBS/length不一致やmessage type不一致が発生した証拠はない。過去RUNでは同経路のACKが成立している。
- 安全：VESC/PCA9685出力なし、COM3未操作。コード変更・ビルド・書込み・実機試験はこの段階では行っていない。
- 次回：配線（Core GPIO9 TX→XIAO D6 RX、XIAO D7 TX→Core GPIO8 RX、GND共通）確認後、reset/START/STOP/ACKだけの短い通信確認。失敗時は10秒試験を開始しない。

## 2026-08-03 RUN0054後の修正・RUN0055短時間確認

- 実施：XIAO bnoTaskスタックを4096→8192 wordへ増量。ESKF predictの15x15行列と診断traceでstack canaryが発生したための最小修正。
- 検証：XIAO/Core両ビルド成功、COM4/COM6へ書込み、MAC確認とhash verified。
- 試験：RUN0055。HTTP202、XIAO/Core START/STOP ACK各1、ESKF reset_count=1、sequence gap/CRC/COBS/length/RX full=0、XIAO stack overflow=0、logger FINALIZED q=0、flush/close/TXT成功。
- 注意：logger診断にpartial_writes=1が残るため、次の10秒試験で全512 B writeとpartial/zeroを再判定する。
- 証跡：pc-tools/boat_eskf/captures/CONTROL_LINK_CHECK2_20260803/。


## 2026-08-03 RUN0056/RUN0057 raw BNO 10秒

- RUN0056：kind段階一致、BIN 4623件/trailing0、finalize成功。ただしLOGGER_TIMING partial_writes=2。SDTRACE実体は全write actual=512で、commit()がn<512の最終チャンクをpartialと誤カウントしていた。
- 修正：Core commit() のpartialカウンタをctual>0 && actual<nに限定。両ビルド成功、Core COM6へhash verified書込み。
- RUN0057：HTTP202、自動停止、P1/STOP ACK、kind1..5=1263/999/499/250/500がXIAO event/TX/Core/BINで一致。UART/report sequence欠落/重複/逆順0、CRC/COBS/length/unknown/RX full/drop/SD error/partial/zero=0、queue=0、flush/close/TXT/FINALIZED、BIN 4620件/trailing0。
- 証跡：pc-tools/boat_eskf/captures/BNO_ALL_RAW_ENQ_10S_RETRY_20260803/。


## 2026-08-03 �ʏ�^�p�����ؑ�

- COM4�iMAC 34:85:18:AB:FA:90�j�݂̂�BOAT_EXPERIMENT=23���r���h�E�����݁B�r���h�����AHash verified�BCOM3�͑��삹���ACOM6�͕ύX�Ȃ��B
- COM4�N�����8�b�̐��V���A����ۑ��BBNO ready=1�Akind1/2/4�L���Akind3/5�����B����Hz��126.36/100.05/25.02�B
- Core���ڑ���Ԃ̂���XIAO���S��Ԃ�FAULT�B���m�F������Core�ڑ���Ɏ��{����B
- �ؐ�: pc-tools/boat_eskf/captures/NORMAL_OPERATION_BOOT_CHECK_20260803/xiao_boot_serial.log


## 2026-08-03 �ʏ�^�p�Î~���Core����

- COM6���J�����Î~����s��USB_UART_CHIP_RESET���API��~�B���񎎍s�͕s�����Ƃ��ĕۑ��B
- CoreS3�S�C���[�W��ROM no-stub/DIO��COM6�֍ď����݁iMAC 30:ED:A0:D4:BF:40�j�AHash verified�BCOM3������B
- COM6���J�����AAPI�݂̂�ESKF reset queued=true�Areset_count=1�A��12�b�̐Î~API��ۑ��Bsequence gap/CRC/COBS/length=0�B
- quaternion/RPY�͑S���_��identity/0 rad�B�ʏ�ݒ�ł�Game Rotation Vector�����Amount���m��̂��ߎ��m�F�����͖����{�B
- �ؐ�: pc-tools/boat_eskf/captures/NORMAL_OPERATION_STATIC_BASELINE_20260803/


## 2026-08-03 RUN0058 GVR��������

- COM4��BOAT_EXPERIMENT=21�������݁BCore COM6��API�̂ݎg�p�ACOM3������B
- RUN0058: kind2 Gyro=999�Akind3 Game Rotation Vector=999�ABIN/TXT��������Atrailing=0�Aqueue/SD/UART�G���[0�B
- GVR�Î~����Roll/Pitch/Yaw=-174.851632/1.461164/-178.638889 deg�Aquaternion norm����0.999994�B
- raw sensor timestamp�t�s��kind2=354�Akind3=371�Breport sequence�͐���B
- ����]����͂܂��s���Ă��Ȃ����߁A���m�F�͖������B


## 2026-08-03 現在状態確認

- COM3には触れず、Core COM6を開かずに /api/link と /api/eskf を確認。
- link: connected、sequence gap/CRC/COBS/length=0。raw kind2/3はRUN0058終了後のためstale。
- ESKF: imu_stale、run_state=0、shadow_only=true、actuator_output_enabled=false。
- 軸回転試験は未実施であり、未完了として記録。次回はユーザーの静止準備完了後に一軸ずつ実施する。

## 2026-08-03 RUN0059 姿勢軸確認

- 準備完了後、Core COM6を開かずAPIで60秒raw取得を開始。COM3は未操作。
- RUN0059: normal_stop=1、records=17721、BIN trailing=0、queue_drops=0、sd_write_errors=0、UART CRC/COBS/length=0。
- type3/type4 rawは各5888件。callback timestamp単調増加0、実効約98.13 Hz、四元数ノルム平均0.999994。
- 25秒以降にRoll/Pitch/Yawの大きな変化を観測したが、約1.15秒のcallback欠測とreport sequence 18→0のリセットが1回発生。
- 通信・SD経路は合格、姿勢軸・符号判定は部分成立として保留。BNO再初期化原因は未確定。
- 証跡: docs/BNO_AXIS_TEST_RUN0059_20260803.md、pc-tools/boat_eskf/captures/BNO_ATTITUDE_AXIS_60S_20260803/

## 2026-08-03 RUN0060 Roll軸切り分け

- 20秒自動取得を実施。COM3未操作、Core COM6はAPIのみ。
- RUN0060: records=5963、type3/type4各2000、normal_stop=1、BIN trailing=0。
- queue_drops=0、sd_write_errors=0、UART sequence gap/CRC/COBS/length=0、queue high-water=22。
- report sequence不連続0、callback timestamp単調増加。RUN0059の約1.15秒欠測・report resetは再発なし。
- 5–15秒でEuler Pitchが約+85度まで変化。物理Roll操作とセンサEuler軸の対応は未確定。
- 次はPitch、Yawを個別に切り分ける。

## 2026-08-03 RUN0061 Pitch軸切り分け

- 20秒自動取得を実施。COM3未操作、Core COM6はAPIのみ。
- RUN0061: records=5989、Gyro=2002、GVR=2003、normal_stop=1、BIN trailing=0。
- queue_drops=0、sd_write_errors=0、UART sequence gap/CRC/COBS/length=0、queue high-water=31。
- report sequence不連続0、callback timestamp単調増加。RUN0059の欠測・report resetは再発なし。
- 物理Pitch操作時にEuler Rollが約-176→+99度変化、Euler Pitchは約0–2度。取付姿勢・軸変換による軸入替え候補を記録。
- 次はYaw単独試験。

## 2026-08-03 RUN0062 Yaw軸切り分け・通常周期復帰

- 20秒自動取得を実施。COM3未操作、Core COM6はAPIのみ。
- RUN0062: records=6147、Gyro/GVR各2002、normal_stop=1、BIN trailing=0。
- queue_drops=0、sd_write_errors=0、UART sequence gap/CRC/COBS/length=0、queue high-water=34。
- report sequence不連続0、callback timestamp単調増加。RUN0059の欠測・report resetは再発なし。
- 物理Yaw操作でEuler Yawが主に変化。3軸の暫定対応（Roll→Euler Pitch、Pitch→Euler Roll、Yaw→Euler Yaw）を記録。
- BOAT_EXPERIMENT=23をCOM4へビルド・書込み。復帰後Core APIでimu age=636 us、link error=0、ESKF run_state=2/health=1/mount_unvalidatedを確認。
- COM3は引き続き未操作。

## 2026-08-03 作業終了

- RUN0058–RUN0062の全証跡と解析をGit管理下へ保存。
- 最終XIAOはCOM4のBOAT_EXPERIMENT=23、COM3未操作。Core COM6シリアル未接続。
- 3軸の暫定対応は物理Roll→Euler Pitch、物理Pitch→Euler Roll、物理Yaw→Euler Yaw。
- 最終Core APIはlink error=0、ESKF run_state=2/health=1/mount_unvalidated。
- 本日の作業を終了し、次回手順をdocs/WORK_HANDOFF_20260803_FINAL.mdへ記録。

## 2026-08-04 RUN0063候補マウント変換試験と復旧

- RUN0060～0062の符号付きgyroから暫定推定した `bodyX=-sensorY, bodyY=sensorX, bodyZ=sensorZ` をXIAOのESKF入力へ一時反映。`kBnoMountValidated=false`は維持。
- exp23のビルド成功（RAM 204308/327680、Flash 576389/3342336）。COM4（MAC 34:85:18:AB:FA:90）へ書込み・Hash verified。COM3は未操作。
- RUN0063開始APIはHTTP202、`eskf_reset_queued=true`。RUN0063.TXTはnormal_stop=1、records=1251、queue_drops=0、sd_write_errors=0、control_cobs_errors=1。
- 試験中のCore APIは受信フレームが停滞し、ESKF `alignment_incomplete/run_state=1/health=0`。候補変換の正否は判定不能として不成立。
- 候補変換をソースから除去し、恒等行列・raw ESKF入力へ復旧してCOM4へ再書込み。復旧後はlink connected、sequence gap/CRC/length=0をAPIで確認。ESKFはalignment_incompleteのため合格扱いしない。
- 証跡: `docs/BNO_MOUNT_CANDIDATE_STATIC20_20260804.md`、`pc-tools/boat_eskf/captures/BNO_MOUNT_CANDIDATE_STATIC20_20260804/`

## 2026-08-04 ESKFリセット経路診断とXIAOスタック修正

- ユーザー指示に従い、BOAT_EXPERIMENT=23、mount transform未確定、COM4 XIAO／COM6 Coreのみで作業。COM3は開かず、P1/RUN0064も開始しなかった。
- Core側でAPI受付、TX enqueue、TX complete、ACK受信、ACK timeout、reset_count観測を追加。XIAO側でEskfCommandの受信判定と既存CommandAckによる結果通知を追加。BNO周期、ESKF方程式、ログ形式、アクチュエータ設定は変更していない。
- `/api/eskf/reset` を1回送信し、応答は `queued=true`。Core traceは `api_requests=1, core_tx_enqueued=1, core_tx_complete=1, ack_received=0, ack_timeout=1`、reset_countは0のまま。UART sequence gap/CRC/COBS/lengthは0/0/0/0。
- XIAOログで `Guru Meditation` / `Stack canary watchpoint triggered (loopTask)`。addr2lineは `eskf::Shadow::updateLinear`（eskf.cpp:97）→`updateGnss`（110）→`processNav`（main.cpp:105）→`linkRxService`（157）→`loop`（177）を示した。ACK未受信の第一原因をloopTaskスタック枯渇と判定。
- `xiao-boat-control-integration/platformio.ini` に `ARDUINO_LOOP_STACK_SIZE=16384` を追加。Core/XIAO両方をビルドし、Core COM6はROM no-stub/DIO、XIAO COM4へ書込み、MAC/hash確認。COM3は操作していない。
- 修正後、リセットAPIは再送せず起動のみ約8秒確認。stack panicなし、BNO event A/G/Mが増加、TX drop/partial/zero=0、Core API linkもsequence gap/CRC/length=0で復旧。XIAO FAULTは安全状態でありACK成功とは扱わない。
- 証跡: `docs/ESKF_RESET_FLOW_DIAGNOSTIC_20260804.md`、`pc-tools/boat_eskf/captures/ESKF_RESET_FLOW_DIAGNOSTIC_20260804/`。
## 2026-08-04 提言書実現可能性静的解析

- 添付方針を確認し、優先順位を提言書の2台XIAO実現可能性確認へ変更。ESKF reset再試験、RUN0064、mount候補再適用、15状態ESKF本番接続は実施しない。
- origin/main `fa5a73b8bef303c46560c2fb16658ded4f6ef97a`を正本として解析。制御側はBno task（core0, priority3, 8192 words）、LinkTx（core0, priority4, 4096 words）、Arduino loop（core1, loop stack 16384 bytes）。通信・記録側CoreはUartRx（core1, priority2, 8192 words）、SdWriter（core1, priority1, 12288 words）、Arduino loop（core1）。
- XIAOはBNO event queue 96、link TX queue 64、Core logger queue 96、Core UART RX buffer 16384 B、SD buffer 8192 B/512 B chunk、BNO I2C 100 kHz、周辺I2C 400 kHz。BOAT_EXPERIMENT=23はAccel/Gyro 100 Hz、Mag 20 Hz、GVR/Linear無効。
- P0、BOAT_EXPERIMENT=23、BNO all実験30の静的ビルドを実施し、全て成功。P0/23 RAM 205076 B、Flash 577381 B、all RAM 205076 B、Flash 577417 B。Core既存ビルドはRAM 168156 B、Flash 1034237 B。
- 提言書機能を静的分類。BNO/GNSS/ToF/SD/UART骨格は存在するが、AS5600、Waypoint/LOS/ILOS、水平EKF、高さKF、大会状態機械、実制御経路は未実装または別途SHADOWが必要。最終判定A/B/C/Dは保留。
- 構成Cを最初の候補、構成Bを比較候補とした。UART帯域の机上見積りはBNO allだけで約213–218 kbps（921600 bps中）だが、burst/queue/SD/deadlineを含む実測が必要。
- 証跡文書: `docs/PROPOSAL_FEASIBILITY_STATIC_20260804.md`。実機・COM3操作なし。