# 大会向け提言書：2台XIAO ESP32S3実現可能性 静的解析

実施日: 2026-08-04  
正本: `origin/main` `fa5a73b8bef303c46560c2fb16658ded4f6ef97a`  
解析ブランチ: `agent/bno-int-telemetry-handoff`（ソース内容は上記mainと同一）  
対象: `xiao-boat-control-integration`、`m5stack-cores3-telemetry-bridge`

## 1. 今回の方針変更

従来のESKF reset再診断、RUN0064、候補BNO軸変換、15状態ESKFのalignment修復は中断した。現行15状態ESKFは削除せず、研究用・SHADOW専用として保存する。今回の目的は、提言書の処理を2台のXIAO ESP32S3でセンサ取得・UART・SD保存・安全監視と同時実行できるかを、まず静的に判定することである。

実アクチュエータ、水上試験、提言書全機能の本番接続、BOAT_EXPERIMENT=23の上書きは行っていない。COM3も操作していない。

## 2. ボード・ビルド条件

|項目|制御側XIAO|通信・記録側CoreS3|
|---|---|---|
|MCU|ESP32-S3|ESP32-S3|
|CPU|240 MHz（board定義）|240 MHz（board定義）|
|Flash|8 MB|16 MB|
|内部RAM上限|327,680 B|327,680 B|
|PSRAM|`BOARD_HAS_PSRAM`、BNO timestamp集合をPSRAMへ割当|`BOARD_HAS_PSRAM`|
|PlatformIO|espressif32 6.12.0|espressif32 6.7.0|
|Arduino framework|3.20017.241212+sha.dcc1105b|3.20016.0|
|IDF|ビルドパッケージ付属定義。XIAOの個別IDF文字列は未採取|コンパイル定義に`IDF_VER=v4.4.7-dirty`|
|安全設定|`kDryRunActuators=true`, `kShadowOnly=true`, `kActuatorOutputEnabled=false`|アクチュエータ出力なし、ログ・Web・GNSS専用|

## 3. 現行タスク構成

### 制御側XIAO

|処理|実行core|priority|stack指定|内容|
|---|---:|---:|---:|---|
|Arduino `loop()`|core 1（board定義）|framework|`ARDUINO_LOOP_STACK_SIZE=16384` bytes|UART RX、ToF、INA、VESC受信、状態機械、Web、診断|
|`Bno`|core 0|3|8192 words（実装指定）|BNO INT通知、`imu.poll()`、BNO callback処理、ESKF predict、状態送信|
|`LinkTx`|core 0|4|4096 words（実装指定）|Heartbeat、UART送信queueのdrain|
|BNO ISR|割込み|—|—|INT edgeでBno taskを通知。I2C処理はISR内で行わない|

UART RXは独立taskではなく、`loop()`内の`linkRxService()`で処理する。BNO callbackはevent queueへ投入され、Bno taskが取り出す。過去に15x15行列処理でstack canaryが発生したため、loop stackは16 KBへ修正済みだが、提言書全機能を同時実行した高水位は未計測である。

### 通信・記録側CoreS3

|処理|実行core|priority|stack指定|内容|
|---|---:|---:|---:|---|
|Arduino `loop()`|core 1（board定義）|framework|framework既定|M5.update、Web、GNSS、制御UART送信、画面・診断|
|`UartRx`|core 1|2|8192 words|制御側UARTの受信、COBS/CRC/length検査、frame dispatch|
|`SdWriter`|core 1|1|12288 words|queue dequeue、BIN encode、512 B SD write、flush/close/TXT finalize|

CoreS3はSDを10 MHzで初期化し、8 KBバッファを512 B単位で書く。LCD表示はlogging/logFinalizing中停止する既存条件を維持している。

## 4. queue、mutex、バッファ、I/O

|項目|現行値・実装|
|---|---|
|BNO event queue|depth 96、非ブロッキング投入。満杯時はevent dropとして計数|
|XIAO link TX queue|depth 64、最大payload 768 B。満杯時はlink drop|
|Core logger queue|depth 96 frame。満杯時は`queueDrops`|
|Core control UART RX|921600 bps、UART RX buffer 16,384 B、service budget 2,048 B/32 frame/2 ms|
|GNSS UART|115200 bps、RX buffer 2,048 B、1回512 B budget|
|SD buffer|8,192 B、write chunk 512 B|
|排他|XIAO: `linkMux`, `bnoMetricsMux`, ESKF内部mux。Core: `queueMux`, `cacheMux`, `sdMutex`, trace mutex|
|I2C|BNO専用Wire 100 kHz。ToF/PCA/INA共用Wire 400 kHz（現行23ではINA無効）|
|UART protocol|COBS delimiter + CRC32、packed header 22 B、payload最大768 B|

## 5. 現行周期（コード上の確定値）

|機能|現行BOAT_EXPERIMENT=23|提言書候補|
|---|---:|---:|
|BNO Accelerometer|100 Hz（10,000 us）|100 Hz|
|BNO Gyroscope|100 Hz（10,000 us）|100 Hz|
|BNO Magnetic|20 Hz（50,000 us）|20 Hz|
|BNO Game Rotation Vector|無効|50 Hz（20,000 us）|
|BNO Linear Acceleration|無効|50 Hz（20,000 us）|
|BNO service fallback|2 ms、INT低時は最大8回`sh2_service()`|同じ|
|ToF|profile 3を使用する現行23では16 zone/30 Hz設定。提言書最低構成の8×8/10 Hzは別profile 0が必要|8×8/10 Hz|
|INA226|無効（`kEnableIna226=false`）|約50 Hz候補。ただし実配線・有効化未確認|
|PCA9685|50 Hz設定|20–50 Hz制御候補|
|VESC poll|20 ms request|本番未接続・DRY_RUN|
|ESKF state/health|50 ms / 200 ms、SHADOW送信|研究用のみ、提言書本番経路には未接続|
|Heartbeat|100 ms|安全監視候補|
|GNSS nav|通信側生成100 ms周期|実測更新は過去約7 Hz|

`BOAT_EXPERIMENT=30`（BNO all 5 reports）のコード上設定は存在し、今回の実機書込みはしていない。

## 6. ビルド資源比較（静的）

同一ソースをfeature設定だけ変えてビルドした。P0、安定版23、全BNO設定30は、現時点では同じ処理骨格を条件分岐するため、静的RAMは同一である。

|環境|結果|RAM|Flash|
|---|---|---:|---:|
|`p0_bringup_400k`|成功|205,076 / 327,680 B (62.6%)|577,381 / 3,342,336 B (17.3%)|
|`bno_accel100_gyro100_mag20_int_3min`（23）|成功|205,076 / 327,680 B (62.6%)|577,381 / 3,342,336 B (17.3%)|
|`bno_final_all_100_100_20_50_50_10s`（30）|成功|205,076 / 327,680 B (62.6%)|577,417 / 3,342,336 B (17.3%)|
|CoreS3 `m5stack-cores3`|成功|168,156 / 327,680 B (51.3%)|1,034,237 / 6,553,600 B (15.8%)|

これはリンク時の静的使用量であり、heap高水位、PSRAM使用量、task stack高水位、最大処理時間を含まない。したがって「実現可能」の判定ではない。

## 7. 提言書機能の現在実装状況

|機能|現在の実行側|状態|評価上の扱い|
|---|---|---|---|
|BNO raw、Quaternion、Euler|制御XIAO|kind 1–5の取得・変換・UART送信骨格あり。GVR/Linearは実験30のみ|専用実験・SHADOWで評価|
|GNSS受信・raw/TXT/BIN|通信CoreS3|実装済み|継続利用候補|
|ToF 8×8処理|制御XIAO|ライブラリ処理あり。現行23はprofile 3（16 zone/30 Hz）|profile 0の個別benchmarkが必要|
|INA226|制御XIAO|コードあり、現行設定は無効|配線・有効化後に評価|
|AS5600|—|現行ソースに実装確認できず|未実装|
|Waypoint/LOS/Leaky ILOS|—|現行ソースに実装確認できず|未実装。P0候補として別実装|
|水平5状態EKF|—|提言書用の実装なし。15状態ESKFは別のSHADOW|SHADOW骨格を別featureで作る|
|高さ2状態KF|—|実装確認できず|最低構成の中央値/LPFと分離評価|
|Roll/Pitch PD・翼合成|—|現行は試験用servo/VESC profileのみ|実機接続禁止。SHADOWのみ|
|Yaw LOS/gyro rate|—|本番制御経路なし|後段P2候補|
|状態機械|制御XIAO|安全状態・dry-run試験状態はあるが大会状態機械ではない|P0前に設計確定|
|全raw/推定/制御内部ログ|通信CoreS3|既存BIN/TXT・UART診断あり|項目追加時もrawを削減しない|

## 8. UART帯域の静的見積り

`BnoPayload`は56 B、header 22 B、CRC 4 BでCOBS前の最小82 B、delimiterを含む送信量は概ね83–85 B/frameである。提言書BNO 5種（100+100+20+50+50 = 320 frame/s）だけで概ね26.6–27.2 kB/s（約213–218 kbps）。Heartbeat、ToF、推定状態、ESKF診断を加えても921600 bpsの平均帯域には余裕がある見込みだが、これはburst、UART driver、queue滞留、SD同時実行を含まない机上値である。

Core側の過去実測SD平均約36.2 kB/sは、UART平均値だけでなくBIN/TXT処理を含む。実現可能性判定には、frame実測bytes/s、queue high-water、write latency、deadline missを同一試験で取得する必要がある。

## 9. 構成A/B/Cの暫定比較

|構成|内容|静的判定|
|---|---|---|
|A|提言書の推定・航法・制御をすべて制御側XIAO|未判定。現行コードに航法・P0制御がなく、15状態ESKFのstack履歴もあるため、同時実行benchmark前に採用不可|
|B|制御側XIAOはBNO/ToF/高速制御、通信側Core/XIAOはGNSS/Waypoint/LOS/Web/SD|推奨候補。UART帯域の静的余裕はあるが、GNSS航法結果と安全停止の遅延を実測する必要|
|C|大会最低構成を制御側、水平EKF/高さKFは無効またはSHADOW|最初の実現可能性試験に最適。提言書P0/P1の段階導入と整合|

暫定推奨はCから始め、航法を通信側へ分散するBを比較すること。Aは、固定長benchmarkとtask stack高水位で余裕を確認するまで正式採用しない。

## 10. 未実測項目と次の試験

今回確定したのは、主にコード配置・静的RAM/Flash・周期設定・通信形式であり、実時間実現可能性は未確定である。次は実機へ提言書用ファームを書き込まず、既存のbenchmark protocolと専用feature flagを使用する。

1. 現行23の起動・静止SHADOW 60秒で基準値を取得（既存ログを上書きしない）。
2. 保存済みBNO/GNSS/ToFまたは模擬入力で、Quaternion、LOS、固定長EKF候補を10,000回以上benchmarkする。
3. 提言書BNO all設定を専用実験番号で、actuator出力0の10秒→60秒へ段階評価する。
4. `uxTaskGetStackHighWaterMark`、free heap、largest free block、処理平均/max/p99/p99.9、deadline miss、UART bytes/s、queue high-water、SD write latencyを記録する。
5. P0最低構成、P1水中翼SHADOW、完成構成SHADOWを別試験に分ける。

合格判定は、build成功だけではなく、watchdog/brownout/restart/stack overflow/allocation failure/NaN/Inf、queue継続増加、raw欠落、UART/SD欠落、100 Hz 10 ms deadline、50 Hz 20 ms deadline、10 Hz 100 ms deadline、10分heap低下なしを同時に満たした場合に限る。

## 11. 現時点の判定

現時点ではA/B/C/Dの最終判定を出さない。静的解析からは、C→Bの段階構成が最も安全で、Aは未実装機能とstack余裕のため未評価、Dとする根拠もまだない。正式採用、SHADOWのみ、処理分散、周期低下、未確認を機能ごとに後続試験で分類する。

従来ESKF作業を再開する条件は、提言書実現可能性のP0/P1評価方針が確定し、ユーザーが再開を指示した場合である。

