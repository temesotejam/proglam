# 作業計画

最終更新: 2026-08-01

## 現在の段階

- [完了] CoreS3を一時的な通信・SD・GNSS側として稼働（UART 921600 bps、Core GPIO8=RX/GPIO9=TX、COBS+CRC）。
- [完了] RUN0001の再解析。実録時間は92.853619秒であり、60秒ではない。
- [完了] BNO08XからESKFへの入力停止を診断し、原因を制御XIAO側だけで修正。
- [完了] BOAT_EXPERIMENT=24（加速度・ジャイロ100 Hz、磁気20 Hz）をCOM4のXIAO ESP32S3へ書込み、ハッシュ照合を確認。
- [完了] RUN0002静止ログを取得・回収・解析。実録時間は74.233763秒。
- [完了] health=Degradedの理由を特定し、BNOイベント時刻・生IMU統計と60秒自動停止ログを実装・実機起動確認。`mount_valid=false`がDegradedの直接理由。
- [実機待ち] 水平固定を確認後、正確な60秒静止ログを取得する。
- [保留] 静止ログの解析後にBNO一軸ずつの軸・符号試験。ToF較正とGNSS屋外試験には進まない。

## 次の作業

1. 静止時の大きなNED位置・速度ドリフトを、座標系・重力符号・初期加速度バイアスの観測として切り分ける。ESKF数式・ノイズ・取付設定は、根拠が得られるまで変更しない。
2. 上記が解決した後、ToF姿勢試験、GNSS屋外試験、BNO軸試験を順に実施する。
## 2026-08-01 追記 — 静止試験のSD安定化

- [完了] CoreS3のSD書込みタスク `SdWriter` のスタックオーバーフローを実機ログで特定し、4KBから12KBへ拡張した。書込みタスクはメインループと同一優先度、各反復で1ms譲渡する。
- [完了] SD記録中は一覧・ダウンロードAPIを拒否し、停止処理はSD書込みタスクへ一元化した。
- [保留] Core WebServerが再起動後の最初のHTTP要求しか安定応答しない。静止60秒本測定は、Core再起動後の最初の要求として開始し、停止後に再起動してSDから回収する。- [保留] RUN0012は開始応答後に0 byteとなり無効。60秒中のCoreシリアル監視で停止原因を確定してから、静止本測定を再実施する。- [完了] 60秒ログ中のCoreシリアルを取得し、停止原因を特定した。次はmicroSDの接触・給電・SPI信号を点検し、必要ならSDクロックを下げる変更の可否をユーザーへ確認する。- [完了] SDカード抜き差し後も10秒ログでCMD18→CMD0D/CMD00失敗を再現。接触のみでは解消しない。次の修正候補はSPIクロック25MHzを低下させることだが、未実施。
- [不合格・原因未確定] CoreS3のSD SPIクロック10 MHz版で、フォーマット後の10秒ログは1,719レコードで通過したが、続く静止60秒ログは411レコードでCMD18、その後CMD0D/CMD00失敗により停止した。フォーマットでは持続書込みを保証できない。次は別のmicroSDカードで同じ60秒試験を行い、カード本体とCoreS3のカードソケット・3.3 V電源・SPI信号品質を切り分ける。

- [完了] 添付指示に基づき、過去のUART→SD保存成功実績版（RUN0007、121.377秒）とCoreS3現行版を比較し、SDアクセス経路・File共有・キュー・バッファ境界を監査した。
- [実施中] 現行CoreS3へSD時系列診断版をCOM6書込み済み。別microSDカードでの60秒試験ではSDTRACEを回収して、software経路とカード/ハードウェアを切り分ける。

## 2026-08-02 追記 — 現行microSD・SDTRACE 60秒静止試験

- [実施済み・証跡不全] ユーザー指定どおり、現行microSDとSDTRACE版のまま60秒静止ログを1回だけ実行した。SDクロック、カード、バッファ、mutex、Web処理、ログ形式は変更していない。
- [未達] USB再列挙により開始〜終了のアプリシリアル取得が欠落し、SDTRACE/CMD行は0行しか回収できなかった。`rec=7971`、`log=0`、`q=3`は観測したが、`RUN0003.BIN`は0 byte、TXTは未生成である。CMDエラー有無・順序の判定はできない。
- [保留] この一回を再試験として扱わず、次の試験の可否と、USBリセット前から終了後まで単一の取得器を維持する方法をユーザーと確認する。詳細と全保存物は `docs/SDTRACE_STATIC60_TEST_20260802.md`。
## 2026-08-02 追記 — SDTRACE再試験（RUN0004）

- [不合格・分岐B] 現行microSD・10 MHz・現行SDTRACEファームウェアを変更せず、CoreS3を再起動せずに60秒要求を1回実行。アプリ診断3行を確認した持続シリアル記録で、`RUN0004`の途中書込み失敗を完全捕捉した。
- [確定] 512 byte writeは769回成功後、`rec=2880`で770回目が0 byte。SDライブラリは`ff_sd_status(): Check status failed`を3回出力した。直後にwriterがflush/close/TXT open/write/closeを成功として記録したが、`q=5`を残してfinalizeを完了した。BINは0 byte、TXTは257 byteである。
- [保留] 次は変更を加える前に、持続書込み途中のSD状態喪失と、失敗時にqを排出せずfinalizeする状態遷移を切り分ける。詳細と証跡は`docs/SDTRACE_STATIC60_RETRY_REPORT_20260802.md`。
## 2026-08-02 追記 — LCD停止版の書込み待ち

- [完了] `logging || logFinalizing` の間だけ `draw()` を呼ばない変更を実装。`M5.update()`、UART、Web API、SDTRACE、SD設定、ログ形式は不変。CoreS3ビルド成功（RAM 148,556/327,680、Flash 1,017,013/6,553,600）。
- [書込み済み・実機待ち] COM6へROM no-stub書込みし全イメージhash verified。しかしhard reset後にCOM6がWindowsへ再列挙されずCOM4のみ検出。アプリ診断3行の確認前であり、60秒試験は未開始。
## 2026-08-02 追記 — LCD停止でのSD連続記録合格

- [完了] CoreS3では`logging || logFinalizing`中に`draw()`を停止し、`M5.update()`は維持する方式を採用。RUN0005の60秒ログは全512 B write成功、SDエラー0、queue drop 0、q=0、auto stop・flush・close・TXT成功、BIN全件復号成功。
- [判断] LCDとmicroSDの共有SPI競合/表示負荷を有力候補として記録。CoreS3向けの複雑なSPI排他は追加せず、画面はログ中停止を暫定方針とする。
- [次] 本来のBNO、ToF、GNSS試験へ進む。XIAO移行後にはXIAO上でSD連続記録を改めて検証する。詳細と証跡は`docs/LCD_OFF_SDTRACE_STATIC60_SUCCESS_20260802.md`。
## 2026-08-02 — BNO08X静止60秒試験の障害後

- [完了・不成立] RUN0005のLCD停止条件を維持して、BNO Game Rotation Vector/Linear Accelerationを各50 Hzで追加したXIAOビルドをCOM4に明示書込みし、静止60秒自動試験を開始した。
- [障害] 開始APIはHTTP 202だったが、その後CoreS3のアプリUSB/Web/ICMPが停止し、70秒超後も停止APIを送れなかった。ROM書込みツールは応答するが、同一ファームウェア復旧書込み後もアプリが起動しない。
- [次] CoreS3を物理的にUSB抜き差しまたは電源再投入してアプリ起動を回復させ、`/api/eskf` と診断3行を確認する。回収不能な本試行は成功回数に数えず、原因を断定しない。証跡: `docs/BNO_STATIC60_ATTEMPT_20260802.md`。

## 2026-08-02 — CoreS3単体切り分け

- [完了] XIAO TX→CoreS3 RXを外し、両機電源断後にCoreS3だけを起動。ログは開始せず、アプリ診断3行以上、SoftAP接続、ping 3/3（開始前2–3 ms、32秒後1 ms）、`/api/eskf`応答、32秒連続動作をすべて確認。
- [判定] 追加BNOデータを送らないCoreS3単体では障害は再現しない。次はCoreS3を停止し、XIAO TX→CoreS3 RXだけを復帰させて、ログを開始せずUART受信下で同じ30秒監視を実施する。

## 2026-08-02 — CoreS3 + XIAO UART受信のみ切り分け

- [完了] XIAO TX→CoreS3 RXを復帰し、ログ開始なしで32秒監視。CoreS3はWeb/pingを維持し、XIAO受信・ESKF更新も継続した。
- [判定] 追加BNO報告を含むUART受信だけでは、前回のCoreS3停止は再現しない。障害候補は「追加データ受信とSDログ記録」の組合せ、または前回の非再現要因へ狭まった。
- [注意] UART CRC/COBSは0だが、開始直後からsequence_gaps累計が大きく、length_errors=1を観測。ログ試験を再開する前に、追加報告を送るXIAO側の全体フレームレートとCoreの受信処理余裕を確認する。

## 2026-08-02 — 追加BNO報告、raw BNO記録なしのSDログ

- [完了] `duration_s=10&bno_capture=0`でRUN0007を自動停止。1,918件、9.983638秒、queue drop 0、SD write error 0、全512 B write/flush/close/TXT成功、BIN全件復号（259,122 byteを末尾なしで消費）。
- [次] 同一条件で`bno_capture=1`の10秒試験を1回行い、raw BNOのCoreS3転送・SD記録を加えた時だけ障害が再現するか確認する。

## 2026-08-02 — raw BNO記録有効10秒ログの再現障害

- [完了・再現] `duration_s=10&bno_capture=1`で、開始直後にCoreS3のアプリ診断とHTTP APIが停止した。ICMPは継続応答し、`bno_capture=0`のRUN0007との明確な差分はraw BNOフレーム受信・ログ投入である。
- [原因候補（コード根拠あり）] CoreS3 `serviceControl()`は`while(controlUart.available())`に処理予算がなく、raw BNOの連続受信中にループが戻らず、`web.handleClient()`、自動停止判定、診断出力へ到達しない。XIAO側にはbyte budgetがあるがCoreS3側にはない。
- [次] ファームウェア変更を行う前にユーザーへこの候補と修正案（Core UART受信にbyte/frame budgetを導入）を提示して承認を得る。修正後は、raw BNO有効10秒→60秒静止の順で再試験する。

## 2026-08-02 — UART受信公平化修正と10秒試験

- [完了] CoreS3の修正前構造を確認。UART受信はArduino loopTask内、`while(controlUart.available())`で無制限。SdWriterはCore 1/priority 1、RXバッファ16 KiB、enqueueは非ブロッキング。
- [完了] CoreS3 `serviceControl()`に最大2048 byte、32 frame、2000 usの受信予算を導入。decoder状態、CRC/COBS/length処理、ログ形式、SD設定、BNO周期、mutexは維持。RXDIAGへ予算・バッファ・loop・heap・reset診断を追加。
- [完了] ビルド成功（RAM 148,668 / 327,680、Flash 1,018,865 / 6,553,600）。COM6へROM no-stub書込み、全イメージhash verified。
- [不合格・一部回復] `bno_capture=1` 10秒試験を実施。HTTP、ping、診断、自動停止、q=0、flush、close、TXT、BIN全件復号は成功したが、RX buffer=16 KiB張り付き、time budget hit継続、CRC/COBS/length=3/77/78、decoded frame不足のためUART合格条件未達。
- [保留] BNO08X 60秒静止試験へは進まない。次はUART受信実byte/frame、ハードウェアoverflow、XIAO BNO送信drop、ログ投入有無を分離する診断を行う。
- 詳細報告: `詳細な報告.md`。証跡: `pc-tools/boat_eskf/captures/BNO_FAIR_RX_BUDGET_LOG10_20260802_RERUN/`。

## 2026-08-02 追記：BNO UART分離10秒診断 RUN0011

- [完了] Coreアプリ診断行を確認後、開始前5秒からfinalize後まで生シリアルを保存した。
- [完了] POST /api/log/start?duration_s=10&bno_capture=1&bno_log_enqueue=0 をHTTP 202で実施し、RUN0011を取得した。
- [完了] SD writer、flush、close、TXT、BIN復号を確認した。
- [判定] XIAO送信drop=0に対し、Core試験区間の有効BNO受信=0、RX buffer最大16 KiB、full観測12回。分岐B（Core UART受信/serviceControl/decoder経路）として60秒試験を保留する。
- [次] SD・ログ形式・BNO周期を変更せず、Core UART受信取り込みの追加診断を行う。
## 2026-08-02 追記 — Core UART専用受信タスク診断

- [完了] RUN0011の帯域値を訂正。XIAO encoded 263,584 byte / 10.002 s = 26,353 B/s、921600 bps 8N1占有率28.6%。
- [完了] 変更前のCore task構造（loopTask内無制限UART、SdWriter Core1/priority1/stack12KiB、RX buffer16KiB）をコードで確認・記録。
- [完了] Core UART専用taskをCore1/priority2/stack8KiB、最大512 byte/read、timeout2 ms、1 tick yieldで追加。UART readerとdecoderを一元化し、loopTaskからserviceControlを除去。SD、ログ形式、mutex、UART設定は不変。
- [完了] `raw` / `decode_count` / `dispatch_no_bno_enqueue` / `dispatch`診断モード、毎秒RX/タスク/stack/loop/SDTASK診断を追加。
- [完了] XIAO型別requested/enqueued/completed/encodedBytesカウンタを追加し、P1開始リセット・停止スナップショットを保存。
- [完了] A RUN0012 raw、B RUN0013 decode_count、C RUN0014 dispatch_no_bno_enqueueを実施し、すべて診断目的に合格。現行診断版C RUN0015も自動停止・SDTASK値取得まで完了。
- [完了] BIN/TXT、Core/XIAO生シリアル、SDTRACE、HTTP応答、XIAO型別送信値をcaptureへ保存。詳細は`docs/UART_RX_TASK_DIAGNOSTIC_REPORT_20260802.md`。
- [保留] BNO kind3=Game Rotation Vector、kind5=Linear Accelerationが全試験0件。kind3/5送信原因をXIAO側で切り分けるまで、BNO queue投入60秒静止試験へ進まない。

## 次の作業

1. XIAOのBNO kind3/5設定・イベント処理・送信分岐を確認し、個別送信試験を実施する。
2. kind3/5受信が成立した後、通常dispatch+BNO queue投入の10秒試験を行う。
3. 10秒完全合格後、機体静止を確認して60秒静止試験へ進む。
## 2026-08-02 追記 — Core型別encoded byte診断の追加（書込み待ち）

- [完了] Core `TRIALTYPE`へCOBS delimiter込みの実受信encoded bytesを型別出力する診断を追加。XIAO送信byte数との比較を可能にした。
- [完了] ビルド成功（RAM 154,596/327,680、Flash 1,023,745/6,553,600）。
- [書込み待ち] 自動C試験後にCoreS3 COM6が再列挙されず、現時点はCOM4のみ。最新型別byte版のCOM6書込み・B/C再取得はCoreS3再接続後に実施する。
## 2026-08-02 追記 — 型別encoded byte版の実機書込み・B/C再取得

- [完了] COM6復帰後、型別encoded byte診断版をCoreS3へROM no-stub書込み。全hash verified、書込み後診断を保存。
- [完了] 最新B RUN0016を再実施。trial CRC/COBS/length=0/0/0、型別encoded bytes合計がCore trial rx_bytesと一致、BIN/TXT/SD/finalize成功。
- [完了] 最新C RUN0017を再実施。trial CRC/COBS/length=0/0/0、通常dispatch継続、queue/SD/finalize/BIN成功。終了診断の非原子的境界でTRIALTYPEが1フレーム先行するため、TRIAL_ENDを正本として記録。
- [不成立・除外] RUN0015後のB/C再試験は開始API timeoutでログ開始なし。証跡は保存し、合格数へ含めない。
- [保留] kind3/Game Rotation Vector、kind5/Linear Accelerationは引き続き0件。XIAO BNO設定・イベント送信経路を切り分けるまで60秒静止試験へ進まない。
## 2026-08-02 BNO個別report経路試験の確定更新

- [完了] BNO08X kind1〜5のSH-2 sensor ID/report ID、enableReport interval、sensorId switch、wasReset再設定、イベント経路カウンタをコードで確認し、診断出力を追加した。
- [完了] RUN0024 kind3、RUN0025 kind5、RUN0026 全5reportの10秒試験を実施し、BIN/TXT/SDTRACEとCore/XIAOカウンタを保存した。
- [判定] RUN0025 kind5は指定条件に合格。RUN0024はsequence gap=6、RUN0026はsequence gap=14で不合格。RUN0026はkind1=63.062 Hz、kind4=25.006 Hzも要求値から外れた。
- [保留] BNO enqueue=true dispatch 10秒試験および60秒静止試験。sequence gap=0と全report実効Hz確認後に再開する。
- [注意] RUN0016/17はsequence端点計測追加前の取得であり、first/last sequenceは保存されていない。以後の試験では端点を必須保存とする。
## 2026-08-02 追記 — sequence境界診断とK3/ALL/K1/K4個別10秒試験

- [完了] XIAO共通`uint32_t`採番、START_ACK/STOP_ACKによる同一閉区間、XIAO TX_HISTORY、Core SEQUENCE_GAP（prev/next/欠落範囲/型/RX buffer/decoder/RX task）を実装。
- [完了] Core到着時刻統計 `BNO_CORE_TS`（kind別 first/last、Δt min/avg/max）を追加。UART構造、BNO設定周期、SD設定、バッファ、mutex、ログ形式は維持。
- [完了・不合格] RUN0027 K3：500件、effective 50.014 Hz、drop/error 0、ただし同一区間sequence gap=2（67030/67031、XIAO TX complete履歴あり）。
- [完了・不合格] RUN0028 ALL：kind=630/499/500/250/500、gap=26、K1=63.063 Hz、K4=25.006 Hz。drop/error 0。
- [完了・不合格] RUN0029 K1：設定20,000 us、Core到着631件、Δt 10.265/15.869/33.039 ms、gap=6。
- [完了・不合格] RUN0030 K4：設定50,000 us、Core到着200件、Δt 47.075/49.975/53.863 ms、gap=2。2223/2224はXIAO TX complete履歴あり。
- [保留] K1/K4の試験区間専用sensor timestamp統計、欠落A/B/C/Dの全件照合、dispatch enqueue=1、60秒静止試験。
- [次] `docs/SEQUENCE_DIAGNOSTIC_HANDOFF_20260802.md`の明日手順から再開する。
## 2026-08-03 追記 — K4 timestamp/sequence再診断

- [完了] XIAO P1区間専用の `BNO_TS_TRIAL` を追加。kind 1–5ごとにevents、重複、逆行、sensor/RX間隔、最初/最後を停止時に確定出力する。BNO周期、UART、SD、ログ形式、payload、出力設定は不変。
- [完了] RUN0031で2件の`seq_gap`をXIAO TX_HISTORYと照合。両方とも送信完了（flags=0x07）で、XIAO並行タスクがsequence採番後に別順でTX FIFOへ投入した順不同だった。物理UART欠落ではない。
- [完了] XIAO `linkSend()`を、sequence採番からencode・TX FIFO投入まで同じ短い`linkMux`区間にして順序を保証するよう修正。K4再試験RUN0032でtrial seq_gap=0を確認。
- [完了] RUN0032: K4 TX/ Core receive=200/200、Core到着間隔45,967/49,969.98/53,827 us、CRC/COBS/length=0/0/0、RX full=0、SD error/queue drop=0、flush/close/TXT/BIN全件復号成功。
- [注意] ROM書込みでQIOを明示したためbootloaderヘッダが本来のDIOからQIOへ変わり、XIAOがROM段階でTG0WDTを繰返した。COM4/MAC照合済みXIAOを生成物本来のDIOで再書込みし復旧。以後このXIAOのROM書込みはDIOを使用する。
- [次] K3を同じ順序修正後に再試験し、次にALL 5 reportsでtrial seq_gap=0・型別一致を確認する。両方の合格前にdispatch enqueue/60秒静止試験へは進まない。

## 2026-08-03 追記 — K3/ALL順序修正後の再試験

- [完了] K3再試験RUN0033: trial seq_gap=0、kind3 TX/Core=500/500、Core到着15,389/19,993.76/24,082 us、trial CRC/COBS/length=0/0/0、RX full=0、SD/TXT/BIN成功。
- [完了] ALL再試験RUN0034: trial seq_gap=0、kind1/2/3/4/5のCore受信=631/500/500/250/500、XIAO TX完了=631/500/500/250/500、CRC/COBS/length=0/0/0、RX full=0、queue drop/SD error=0、flush/close/TXT/BIN成功。
- [不合格・BNO周期] RUN0034のXIAO試験区間実効Hzはkind1=63.118、kind2=50.017、kind3=50.016、kind4=25.029、kind5=50.016。kind1/4は設定した50/20 Hzに一致しない。連番順序問題とは分離済み。
- [保留] `bno_log_enqueue=1`のdispatch試験と60秒静止試験には進まない。次はBNO08Xが受理・出力するkind1/4周期を、設定値変更なしの診断で確認し、必要ならユーザー承認後に条件変更を検討する。

## 2026-08-03 update — direct ALL-5 retry

Status: RUN0035 is a valid direct-PC capture and passes UART transport, sequence ordering, SD finalization, and BIN decoding. The earlier retry directory without proxy exclusion is invalid as a test run.

Next: inspect, without changing BNO report configuration, why BNO08X kind1 and kind4 output 63.2/25.0 Hz for requested 50/20 Hz. Keep bno_log_enqueue=0 and do not start the 60-second static data run until this gate is resolved or the user approves changed acceptance conditions. For future CoreS3 HTTP commands, force local bypass with NO_PROXY=* / no_proxy=* or curl --noproxy "*".
## 2026-08-03 — BNO Feature Response受理周期診断

- [完了] 現行BOAT_EXPERIMENT=20の要求周期を変更せず、BNO専用タスク内でSH-2 Get Feature Response（0xFC）を取得する診断を追加してビルドした。
- [実機待ち] 各Set Feature直後、全設定後、wasReset再設定後、およびP1試験開始直後の5種スナップショットを記録する。対象はCOM4のボートXIAO（MAC照合必須）だけであり、COM3は対象外。
- [保留] Feature ResponseとK1/K4/ALLのBNO/XIAO/Core時刻計測に基づく最終要求周期の決定。値の変更にはユーザー承認が必要。
## 2026-08-03 — Feature Response診断の実機結果

- [完了] K1/K4/ALLについて、現行要求値を変えずにFeature ResponseとXIAO/Core到着時刻を取得した。K1は単独/ALLとも62.5Hz受理、K4は単独20Hz・ALL 25Hz受理である。
- [未完了] BNO sensor timestampの形式・wrap・単調性、およびBNO/XIAO/Coreのdelta分布診断。Core type=23 unknown 1件の送信元特定。
- [保留] 上記を終え、最終要求周期をユーザー承認で確定するまで、raw BNO enqueue試験・60秒静止試験へ進まない。
## 2026-08-03 — 最終周期確定・実機試験完了

- [完了] Core type=23を共有enumのP1Capture ACKとして登録し、payload長16、payload hex、CRC/COBS/length、境界情報を診断出力。
- [完了] Core BNO_CORE_EVENTを追加し、kind/report sequence/UART sequence/sensor raw・converted/XIAO rx/queue push/Core rxを保存。
- [完了] SH-2 timestampの意味をコード確認。millis()*1000由来のホスト時刻推定値であり、BNO独立時計ではないことを確定。
- [完了] K1/K4/ALL timestamp診断（RUN0040〜0042）。report sequence、UART sequence、CRC/COBS/length、RX full、type23を確認。sensor timestampの同値/逆行はSH-2 report delay補正とpayload内順序で説明。
- [完了] 最終周期環境27〜30を追加し、4環境ビルド成功。
- [完了] 最終要求周期試験 RUN0043〜0046。Feature Response、実効Hz、SDTRACE、flush/close/TXT、BIN全件復号を保存。
- [注意] ALL同時設定では磁気要求20 HzがFeature accepted 25 Hz（40,000 us）。単独K4は20 Hzを受理。要求値と受理値を分離して通常運用設計へ引継ぐ。
- [次] 通常運用周期の実装・静止60秒試験。COM4 XIAOとCOM6 CoreS3のみ使用し、COM3は触らない。
## 2026-08-03 最終ALL raw BNO保存確認 RUN0047（不合格）

- [完了] COM4/COM6のMACを照合し、COM3を操作せず、現行最終ALL設定のまま指定API（bno_log_enqueue=1、uart_rx_diag=dispatch）を10秒1回実施。
- [完了] 生シリアル、開始API応答、RUN0047.BIN/TXT、SDTRACE、BIN復号結果、段階別件数集計を保存。
- [不合格] XIAO event→TX enqueue→TX complete→Core受信は一致したが、BIN保存がkind1:-3、kind2:-3、kind3:-1、kind4:0、kind5:-1で不一致。finalize後queueも0にならず27で残留。
- [確認] UART seq gap/CRC/COBS/length/unknown/RX buffer full、XIAO drop/partial/zero、SDTRACE write/flush/close/TXT生成はエラーなし。type23はP1Capture START/STOP ACKとして認識。
- [次] writer停止条件、finalize開始条件、キュー排出待ち、BIN保存件数の整合をコードと再現試験で調査する。完全合格まで通常運用周期へ切り替えない。
## 2026-08-03 RUN0048–RUN0051 finalize競合切り分け（完了）

- RUN0048/0049で、Core受信までのraw BNO段階件数は一致する一方、writer終了・File close後にenqueueが受理され、queue残量とBIN欠落が発生する競合を確認した。
- CoreS3へlogger状態機械とenqueueゲートを実装した。STOP ACK受信後にゲートを閉じ、active enqueue=0、queue=0、BIN buffer=0を確認してからwriter終了、flush、close、TXT生成を行う。
- COM6（MAC 30:ED:A0:D4:BF:40）へビルド・ROM no-stub書込み・hash検証済み。COM4（XIAO MAC 34:85:18:AB:FA:90）を併用し、COM3（MAC 44:1B:F6:E2:09:F8）は操作していない。
- RUN0050、RUN0051の同一API 10秒試験が連続合格。kind1..5のXIAO event/TX enqueue/TX complete/Core受信/BIN保存が完全一致し、UART gap・CRC/COBS/length・drop・SD error=0、queue=0、flush/close/TXT/BIN復号が成功した。
- RUN0051保存先: `pc-tools/boat_eskf/captures/BNO_FINALIZE_FIXED_ALL_FINAL_20260803/`。次はこの条件を維持した60秒静止試験を行い、その後に通常運用周期へ切り替える。
## 2026-08-03 RUN0052 VESC/PCA9685取り外し60秒静止ESKF試験

- [完了] VESC（電源・信号・UART）とPCA9685（I2C・電源・サーボ）を全電源断状態で取り外し、再接続なしでCOM4 XIAO/COM6 CoreS3のみを使用した。COM3は未操作。
- [完了] `duration_s=60&bno_capture=1&bno_log_enqueue=1&uart_rx_diag=dispatch`をHTTP 202で実施し、自動停止・STOP ACK・finalizeを確認した。
- [完了] raw BNO kind1..5のXIAO event/TX enqueue/TX complete/Core受信/logger enqueue/BIN保存が全件一致。UART gap、CRC/COBS/length、unknown、RX full、SD error、queue残量は0。RUN0052.BIN/TXTを保存し、BINは27,724 records・trailing=0で完全復号。
- [不合格・ESKF静止性能] quaternion norm、finite、covariance、reset_countは正常だが、5→60秒で速度・NED位置が大きくドリフト（速度+41.30/-125.33/+8.08 m/s、位置+19,899.62/-114,850.13/+2,018.02 m）。predict bad-dt reject増分328、innovation時系列は現行実装/APIで未保存。
- [保留] 通常運用周期切替、アクチュエータ統合、航行試験は行わない。次はESKF開始時リセットを明示した再試験、bad-dt原因、ToF/GNSS未接続時の推定設計、innovation/predict/update診断保存を検討する。詳細は`詳細な報告.md`および証跡フォルダ。

## 2026-08-03 RUN0053 ESKF??10???
- ??????predict dt??????????trace?ESKF reset_at_start?innovation/update counters??XIAO COM4?Core COM6?????????COM3?????
- Core??stub???????COM6 MAC 30:ED:A0:D4:BF:40?????ROM no-stub?????hash verified?
- ??API?HTTP 202?????????????????Core?XIAO?????????P1 START/STOP ACK?????????TRIAL_END?105?????raw BNO 0??STOP_ACK timeout?LOGGER_STATE ERROR?
- SDTRACE?512 B write?flush?close?TXT???????????ESKF??????????????RUN0053??? pc-tools/boat_eskf/captures/BNO_ESKF_DIAGNOSTIC10_20260803/ ??????TX/RX/GND????????10??????????
## 2026-08-03 作業再開：RUN0052/RUN0053監査

- [完了] RUN0052を上書きせず再解析。ロガー/UART/SD/BINは合格、ESKFはreset_count=0、predict reject=328、innovation時系列未保存。
- [完了] RUN0053のCore→XIAO経路をコードと生ログで追跡。Core API送信要求は発生しているが、XIAO側受信フレーム・reset・P1 START/STOP ACKは0件。
- [判定] 最初の失敗位置はXIAO linkUart RX入口前。Core→XIAO物理信号線または接続状態を優先確認する。同じコード経路で過去RUNはACK成立しているため、根拠なしのプロトコル変更は行わない。
- [保留] VESC/PCA9685未接続、COM4/COM6のみ、COM3未操作を維持して配線確認後に短いreset/START/STOP/ACK確認を行う。
- [保留] 双方向確認が合格するまで10秒試験・60秒静止試験・通常運用周期への切替は行わない。
- [次] 配線確認後に短い通信確認、合格時のみ新RUN番号（想定RUN0054）で開始時reset付き10秒静止試験。

## 2026-08-03 追記：RUN0054後のBnoスタック修正とRUN0055通信確認

- RUN0054でXIAO Bnoタスクのstack canary再起動を確認。ESKF 15x15共分散伝播と診断traceの合成スタック不足と判断し、noTaskスタックのみ4096→8192 wordへ増量。周期、UART、SD、mutex、ログ形式、安全設定は維持。
- XIAO COM4/CoreS3 COM6をビルド・書込み。両方hash verified。COM3は未操作。
- RUN0055（duration=1、bno_log_enqueue=0）でSTART/STOP ACK、ESKF reset_count=1、seq_gap=0、CRC/COBS/length=0、queue=0、flush/close/TXT、FINALIZED、stack overflow=0を確認。
- 次は同条件で要求10秒raw BNO試験。partial/zero writeを合格条件として再確認し、10秒合格前に通常運用へ切り替えない。


## 2026-08-03 追記：RUN0056/RUN0057 raw BNO 10秒

- RUN0056は段階件数・BIN復号・finalizeは成立したが、partial_writes=2が正常な最終短チャンクの誤カウントだったため判定保留。Core commit() のpartial判定を実actual<nだけに修正。
- 両プロジェクト再ビルド、Core COM6再書込み後、RUN0057を同一APIで再試験。
- RUN0057はkind1..5のevent/TX enqueue/TX complete/Core受信/BIN保存が各段階一致、UART/report sequence欠落・重複・逆順0、drop/error0、partial/zero0、queue=0、flush/close/TXT、FINALIZED、BIN trailing=0で合格。
- 通常運用周期への切替は未実施。SH-2 raw sensor timestamp重複/逆行とresetなしESKF累積状態は次の評価課題。


## 2026-08-03 �ʏ�^�p�����ւ̐ؑ�

- [����] RUN0057���i��ABOAT_EXPERIMENT=23�i�����x100 Hz�E�W���C��100 Hz�E���C20 Hz�AGame/Linear�����j��COM4�̃{�[�g�pXIAO�փr���h�E�����݁BCOM3�͖�����B
- [�m�F] BNO ready=1�B�N���f�f��kind1/2/4�̂ݗL���Akind3/5��events=0�B����Hz�͖�126.36/100.05/25.02�B
- [��] COM6��ڑ������Î~��Ԃ�BNO Roll/Pitch/Yaw���m�F������s���B
- �ؐ�: docs/NORMAL_OPERATION_SWITCH_20260803.md


## 2026-08-03 �ʏ�^�p�Î~�

- [�s����] ����ESKF reset���s��ACoreS3��USB_UART_CHIP_RESET��SoftAP/API��~�BCOM6�V���A����J������͈Ȍ�����B
- [����] CoreS3 COM6�֑S�C���[�W��ROM no-stub/DIO�����݁AHash verified�BCOM3�͖�����B
- [����] COM6��J����API�݂̂�ESKF reset_count=1�A12�b�Î~API���ۑ��Bsequence gap/CRC/COBS/length=0�B
- [�ۗ�] �ʏ�^�p�ł�Game Rotation Vector�����Emount���m��̂��߁ARoll/Pitch/Yaw���m�F�͖����{�B����������܂��͈ꎞ�f�f�ݒ��m�肷��B
- �ؐ�: docs/NORMAL_OPERATION_STATIC_BASELINE_20260803.md


## 2026-08-03 RUN0058 Game Rotation Vector���m�F����

- [����] ���m�F�pBOAT_EXPERIMENT=21��COM4�֏����݁BGyro/GVR 100 Hz�B
- [����] RUN0058 API����10�b�Bkind2/3=999/999�ABIN 2,976 records�Atrailing=0�AP1 ACK=2�AUART/SD/queue�G���[0�B
- [�ۗ�] �@�̂��]������Roll/Pitch/Yaw���E�����m�F�͖����{�B���͈ꎲ���p����Œ肵�Ď擾���A�I����BOAT_EXPERIMENT=23�֕��A�B
- �ؐ�: docs/BNO_ATTITUDE_GVR_10S_20260803.md�Apc-tools/boat_eskf/captures/BNO_ATTITUDE_GVR_10S_20260803/


## 2026-08-03 現在状態確認

- RUN0058後のCore APIを確認。linkは接続中でsequence gap/CRC/COBS/length=0。
- /api/eskf はraw試験終了後のため imu_stale / run_state=0。shadow_only=true、actuator_output_enabled=false。
- 物理的なRoll/Pitch/Yaw回転は未実施。ユーザーの準備完了後に軸ごとの回転・保持試験を行う。
- 証跡: docs/BNO_AXIS_TEST_STATUS_20260803.md

## 2026-08-03 RUN0059 姿勢軸確認

- [部分成立] 60秒自動停止、BIN/TXT/finalize、SD/UART経路は正常。Gyro/GVR各5888件、queue drop/SD/UARTエラー0。
- [要再試験] callback timestampは単調だが、約1.15秒の欠測とreport sequence 18→0のリセットが1回発生。軸符号は未確定。
- 次は同設定で軸ごとに短時間再試験し、report reset再発を確認してからBOAT_EXPERIMENT=23へ戻す。
- 証跡: docs/BNO_AXIS_TEST_RUN0059_20260803.md、pc-tools/boat_eskf/captures/BNO_ATTITUDE_AXIS_60S_20260803/

## 2026-08-03 RUN0060 Roll軸切り分け

- [合格] Roll単独20秒raw取得。Gyro/GVR各2000件、BIN trailing=0、normal_stop=1、queue/SD/UARTエラー0。
- [確認] report sequence不連続0、callback timestamp単調、約10 ms周期。RUN0059のBNO report resetは再発しなかった。
- [保留] 操作したRoll軸はEulerではPitch成分が主に約+85度変化。取付姿勢・軸変換の確定にはPitch/Yaw単独試験が必要。
- 次はPitch軸、続いてYaw軸を同条件で試験する。
- 証跡: docs/BNO_AXIS_ROLL_RUN0060_20260803.md、pc-tools/boat_eskf/captures/BNO_AXIS_ROLL_20S_20260803/

## 2026-08-03 RUN0061 Pitch軸切り分け

- [合格] Pitch単独20秒raw取得。Gyro 2002、GVR 2003、BIN trailing=0、normal_stop=1、queue/SD/UARTエラー0。
- [確認] report sequence不連続0、callback timestamp単調、約10 ms周期。BNO report reset再発なし。
- [確認] 物理Pitch操作はEuler Roll成分が約-176→+99度変化し、Euler Pitchは約0–2度。取付姿勢・軸変換による入替えを強く示唆。
- 次はYaw軸を同条件で試験し、3軸完了後BOAT_EXPERIMENT=23へ復帰。
- 証跡: docs/BNO_AXIS_PITCH_RUN0061_20260803.md、pc-tools/boat_eskf/captures/BNO_AXIS_PITCH_20S_20260803/

## 2026-08-03 RUN0062 Yaw軸切り分け・通常周期復帰

- [合格] Yaw単独20秒raw取得。Gyro/GVR各2002件、BIN trailing=0、normal_stop=1、queue/SD/UARTエラー0。
- [確認] report sequence不連続0、callback timestamp単調、約10 ms周期。BNO report reset再発なし。
- [確認] 物理Yaw操作はEuler Yawが主に変化。RUN0060–0062で物理Roll→Euler Pitch、物理Pitch→Euler Roll、物理Yaw→Euler Yawの対応を記録。
- [完了] 3軸確認後、BOAT_EXPERIMENT=23をCOM4へビルド・書込み。復帰後Core APIの新周期IMU/ESKF受信、link error=0を確認。ESKFはmount_unvalidated保留。
- 次は取付姿勢のX/Y軸変換を確定し、必要なら通常周期で静止性能を再確認する。
- 証跡: docs/BNO_AXIS_YAW_RUN0062_20260803.md、pc-tools/boat_eskf/captures/BNO_AXIS_YAW_20S_20260803/

## 2026-08-03 作業終了時点

- [保存完了] RUN0058–RUN0062の報告・BIN/TXT/API/解析証跡を保存。
- [完了] 3軸確認後、XIAO COM4をBOAT_EXPERIMENT=23へ復帰。Core APIの通常周期復帰を確認。
- [保留] BNO取付姿勢のX/Y軸変換確定。ESKFはmount_unvalidatedのまま。
- [停止] 本日はここで終了。次回はdocs/WORK_HANDOFF_20260803_FINAL.mdから再開。

## 2026-08-04 BNOマウント候補変換試験 RUN0063

- [不成立] RUN0063で、RUN0060～0062から推定した候補行列 `bodyX=-sensorY, bodyY=sensorX, bodyZ=sensorZ` をESKF predict入力へ一時反映して20秒API試験を開始したが、ESKFは `alignment_incomplete/run_state=1/health=0` のままで、候補変換の妥当性判定はできなかった。
- [復旧完了] 候補行列とinline変換を除去し、BOAT_EXPERIMENT=23の恒等行列・raw ESKF入力へ戻してCOM4へビルド・書込み。MAC確認・Hash verified。COM3未操作。
- [保存完了] RUN0063 BIN/TXT、開始前・試験中・終了後・復旧後のCore API応答を `pc-tools/boat_eskf/captures/BNO_MOUNT_CANDIDATE_STATIC20_20260804/` に保存。
- [保留] 候補変換の再適用、mount_valid=true化、通常運用合格判定は行わない。まずESKFリセット後のalignment遷移と通信再開を切り分ける。
- 証跡: `docs/BNO_MOUNT_CANDIDATE_STATIC20_20260804.md`

## 2026-08-04 ESKFリセット経路診断

- [完了] Core API→Core UART→XIAO受信・dispatch→ESKF reset→ACK→Core APIの診断点を追加。CommandAckの既存wire formatは変更していない。
- [完了] リセット専用試験を1回だけ実施。API受付1、Core TX enqueue/complete 1/1、ACK 0、ACK timeout 1、reset_count 0→0。RUN0064/P1は未開始。
- [完了] XIAO生ログのstack canary panicをaddr2lineで `updateLinear→updateGnss→processNav→linkRxService→loopTask` と特定。
- [完了] 最小修正としてXIAO `ARDUINO_LOOP_STACK_SIZE=16384` を追加し、COM4/COM6へビルド・書込み・hash確認。COM3は未操作。
- [完了] 修正後のCOM4起動確認でpanic再発なし、BNOイベント継続、Core API link診断復旧。リセットAPIは再送していない。
- [次] 修正版でリセット専用APIをもう一度だけ確認し、ACKとreset_count+1が成立した場合のみRUN0064へ進む。
- 詳細: `docs/ESKF_RESET_FLOW_DIAGNOSTIC_20260804.md`、`pc-tools/boat_eskf/captures/ESKF_RESET_FLOW_DIAGNOSTIC_20260804/`
## 2026-08-04 提言書実現可能性確認へ方針変更

- [方針変更] 従来のESKF reset再診断、RUN0064、候補BNO軸変換、15状態ESKF alignment修復を一時中断。現行ESKFは削除せずSHADOW/研究用として保持する。
- [完了] origin/main `fa5a73b8`を正本として、2台XIAO/Core構成、タスク、stack、queue、mutex、周期、I/O、提言書機能の静的解析を実施。実機書込み・COM操作なし、COM3未操作。
- [完了] P0、BOAT_EXPERIMENT=23、BNO all（30）の静的ビルドを実施。全て成功。RAMはXIAO 205076/327680 B、Flashは577381–577417/3342336 B。これは実時間合格を意味しない。
- [完了] 暫定分担は構成C（大会最低構成＋EKF/KF SHADOW）から開始し、構成B（高速制御を制御側、GNSS/Waypoint/LOS/Web/SDを通信側）を比較する方針。A/B/Cの最終判定は未実施。
- [次] 専用feature flag/実験番号で、保存データreplay・固定長algorithm benchmark・P0/P1 SHADOW 60秒・10分試験を段階実施。BOAT_EXPERIMENT=23を上書きしない。
- 詳細: `docs/PROPOSAL_FEASIBILITY_STATIC_20260804.md`

## 2026-08-04 提言書向け固定長ベンチマーク／リプレイ基盤
- [完了] origin/main `f4e2908366ea139c8d55d2de71f4e2595be25438`を確認し、専用ブランチ `feat/proposal-benchmark-replay-20260804` を作成
- [完了] `BENCHMARK_ENABLE`、`REPLAY_ENABLE`、`SHADOW_CONTROL_ENABLE`、`ACTUATOR_OUTPUT_ENABLE=0`、`PROPOSAL_PROFILE`を追加（BOAT_EXPERIMENT=23は維持）
- [完了] BASE/MIN/MID/FULL/LEGACYのホスト固定長10000回ベンチマークと正常・異常リプレイを実装
- [完了] XIAO既存23、専用5環境、CoreS3既存環境を実機書込みなしでビルド
- [完了] 設計書、リプレイ仕様、ビルド比較、次段階SHADOW手順を追加
- [次] PRはDraftでレビュー待ち。XIAO上60秒／10分SHADOW測定後までA/B/C/D判定を保留

## 2026-08-04 MIN SHADOW事前確認（書き込み前停止）
- [停止] USB PnPはCOM4/COM6のみ。保存済み対応ではCOM4が制御側XIAO、COM6が通信側CoreS3で、通信側XIAOを確定できない。COM3未操作。
- [停止] `proposal_replay_min`は識別用feature flagのみで、XIAO実時間MINアルゴリズムへ未接続。ホスト評価コードはファームウェアへリンクされない。
- [未実施] 書き込み、シリアル接続、SD/API、60秒試験。対象機と実行経路が確定するまで再開しない。
- [要選択] 通信側XIAOを接続してMIN経路を実装するか、XIAO+CoreS3の既存仮統合SHADOWを対象とするかを確定する。
## 2026-08-04 ソフトウェア実装結果（最新）

最新方針に従い、実機書込み・COM操作・センサ試験は行わず、Draft PR #18ブランチへ実時間MIN経路を実装した。`shared/proposal_min`を追加し、GNSS局所NED変換、waypoint、LOS/launch yaw、COG妥当性、roll PD、ToF中央値/傾き補正/LPF、高さP、左右前翼＋後部ヨー＋推進shadow、安全状態を固定長で接続した。`proposal_shadow_min`はBOAT_EXPERIMENT=23、SHADOW_CONTROL_ENABLE=1、ACTUATOR_OUTPUT_ENABLE=0、PROPOSAL_PROFILE=1である。

PCA9685/VESCの全出力経路はコンパイル定数と乾式ランタイム条件で二重遮断し、通信側にも同じ出力禁止static_assertと`proposal_shadow_comm`環境を追加した。計測構造体にはtask/operation、queue/UART、sensor age、SD/I2C/heap/watchdog予約、NaN/Inf、STOP/E-STOP/heartbeat、saturation、SHADOW出力count/min/maxを追加した。

検証はC++単体PASS、Python unittest 11件PASS、ホストbenchmark/replay全モード有限値・再現性PASS、XIAO通常環境PASS、XIAO proposal_shadow_min PASS、通信側proposal_shadow_comm PASS、CoreS3既存環境PASS。これらは静的/ホスト検証であり実機合格ではない。実機計測の未接続項目は`docs/PROPOSAL_MIN_UNMEASURED_20260804.md`に記載した。

## 2026-08-04 最新作業状態
- MIN実時間経路、出力ガード、計測、通信側設定、設計文書: 実装済み。
- C++/Python/PlatformIO検証: 完了。
- 実機書込み・COM操作・60秒試験: 未実施（別承認待ち）。
- 次の作業: 未計測項目を実機で収集できる対象機と配線を確定してから、Draft PRレビュー後に別手順で実施。


## 2026-08-04 現行機構MIN対応追記
- [x] 既存4出力候補と旧出力経路を監査（物理マッピング不明を停止条件として記録）
- [x] GNSS→WP/LOS、IMU/ToF→前翼、rear yaw、単一推進のMIN SHADOW計算を共有モジュールへ接続
- [x] neutral/min/max/sign/slew、stale、安全停止、ControlOutput Type 62を追加
- [x] ホスト・制御XIAO・通信XIAO Sense・CoreS3ビルド確認
- [ ] 物理4出力チャンネル、符号、中立、推進方法の実機確定（ハードウェア到着後）


## 2026-08-04 MIN SHADOW運用準備
- [x] Type63/64/65詳細ログ、Core型長、packed static_assertを追加
- [x] PC BINデコーダ/CSV、INA無効表現、VESC ERPM区別を追加
- [x] STOP限定・atomic Waypoint Type66/67と通信側Web UIを追加
- [x] ホスト/C++/Python/PlatformIOビルドを確認
- [ ] 実機60秒/長時間試験、COM操作、書込みはハードウェア不在・指示により保留

## 2026-08-04 PR #18 final hardening (current)

- Status: implementation and host/build verification complete; hardware execution remains pending because no hardware/COM operation is authorized.
- Completed: strict DISARMED-only control-side WaypointSet guard; UI disable guard; temporal INA/VESC decoder; actual 30-minute Controller::step host test; stale/state/safety/slew/STOP-restart checks; Type65 receive timestamp; Python tests and three PlatformIO builds.
- Evidence: C++ host PASS, 90,000 cycles at 20 ms, 270,002 BIN records, decoder all-zero integrity checks, deterministic SHA-256 match, 14 Python tests PASS, compileall PASS, proposal_shadow_min/proposal_shadow_comm/m5stack-cores3 SUCCESS.
- Next: commit and push only feat/proposal-benchmark-replay-20260804, update Draft PR #18 body/comment, do not merge or push main. Hardware test is blocked until explicitly authorized with the correct devices.

## 2026-08-04 PR18 final audit addendum

The final audit is recorded in UTF-8 at `docs/PR18_FINAL_AUDIT_20260804.md`. It covers the shared Type63-67 wire path, six-state C++ Waypoint guard, deterministic 30-minute host run, negative diagnostics, safety-output behavior, test/build evidence, and explicit hardware/COM3 hold. Host-only nonzero propulsion fixtures must not be interpreted as enabling firmware propulsion; firmware remains `SHADOW_CONTROL_ENABLE=1` and `ACTUATOR_OUTPUT_ENABLE=0`.

## 2026-08-05 PR18 corrective audit (current)

添付の最終監査是正指示に基づく現行結果は `docs/PR18_CORRECTIVE_AUDIT_20260805.md` に固定した。正式4出力は左前翼、右前翼、後部ヨー機構、単一推進。Type66は276 bytes、Python診断は19 tests、30分host loopは20 ms/90,000 cyclesである。Type63/64/65/66/67の実測件数は90,000/89,750/89,750/1/1。旧版に記録された14件・15件や旧出力名称は過去履歴であり、現行判定には使用しない。実機・COM3/4/6・main操作は未実施。
## 2026-08-05 PR18 corrective audit supersession

現行の正式な数値と判定は `docs/PR18_CORRECTIVE_AUDIT_20260805.md` を参照する。旧節にあるPythonテスト数、BIN件数、SHA-256、旧HEADは履歴値であり、現行値ではない。現行はPython 38 tests、Type63/64/65/66/67=90000/89750/89750/1/1、合計269502、BIN SHA-256=`9F3B3DFF6519858D131AB5BDF7470CB7FF3D6BC6A55E14D72A273F114FEA4A76`、是正コードHEAD=`fff51e5`。

## 2026-08-05 PR18 最終ホスト監査（現行）

現行の正しいUTF-8監査報告は `docs/PR18_CORRECTIVE_AUDIT_20260805.md`。30分相当は50 Hz / 20 ms / 90,000 cycleで2回実行し、manifest付きreason照合、正式transport診断、CSV各90,000行、INA/VESC freeze復帰、Waypoint本番handler、38 byte goldenを確認した。実機、COM、microSD、upload、mainへの変更は行っていない。

## 2026-08-05 — 大会用SHADOW統合版（着手・未完了）

- [実施中] PR #18のhead `5c6060012c786f66bdee953d28adaf4df7381cb7` から `feat/competition-integrated-shadow-20260805` を作成した。PR #18とmainには変更していない。
- [完了] `shared/competition_shadow` にMANUAL / ATTITUDE_ASSIST / HEADING_HOLD / AUTO_WAYPOINT用の4出力SHADOWコントローラ、モード別センサ依存、slew、STOP/E-STOP、manual timeout、VESC fault、physical gate常時falseを実装した。
- [完了] Type 68〜71（mode/manual/heading/ACK）の後方互換wire定義をXIAO/CoreS3双方へ追加し、XIAOでCRC検証・request ID重複拒否・ACK・SHADOW ControlOutput送信を実装した。
- [完了] `competition_shadow_host` とPlatformIO `competition_shadow` をホスト/ビルド確認した。upload、COM/USB、実機、microSD操作は未実施。
- [未完了] CoreS3 Web/API・同一BINログ統合、長時間2回再現、全回帰build、文書、Draft PR。これらが終わるまで大会用SHADOW統合版を完了としない。
- [完了] 競技用コントローラから内部SafetyStateを削除し、XIAOの正式SafetyStateを一箇所の変換関数から入力する形に変更した。controllerはFAULT/DISARM要求のみを返し、既存XIAO状態機械が遷移を決定する。

- [実施中] Type 68〜70用の64件固定長replay windowを共有モジュールへ追加。RFC1982型比較、完全一致duplicate、ID/sequence衝突、stale、wrapと半周差をホスト試験で確認。XIAO正式受信への接続は未完了。
