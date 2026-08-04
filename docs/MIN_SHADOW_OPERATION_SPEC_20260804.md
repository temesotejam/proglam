# MIN SHADOW 運用準備（2026-08-04）

## 範囲
この段階は feat/proposal-benchmark-replay-20260804 上の MIN SHADOW 経路だけを対象とする。SHADOW_CONTROL_ENABLE=1、ACTUATOR_OUTPUT_ENABLE=0 を維持し、PCA9685・VESC・サーボ・翼への実出力は行わない。MID/FULL/ESKFゲイン調整、センサ性能評価、実機書込みは対象外である。

## ログプロトコル
既存 Type 62 ControlOutput は後方互換の4出力・prelimit・制御量を保持する。追加型は同じ protocol version 1 の packed little-endian payload とする。

- Type 63 ControlSnapshotPayload (190 bytes): cycle/revision、GNSS（lat/lon/speed/course/local N/E）、IMU姿勢・角速度、ToF raw/filtered/height error、u_height/u_pitch/u_roll/u_yaw、front common/differential、4出力・prelimit、validity、state/safety reason、active waypoint。
- Type 64 InaStatusPayload (32 bytes): bus/shunt/current/power、age、valid、error code。INA226が無効または未設定のときは valid=false、errorCode=DISABLED/NOT_CONFIGURED 相当を出し、0を代入して測定値と誤認させない。
- Type 65 VescTelemetryPayload (48 bytes): input voltage、motor/input current、duty、ERPM、温度、tachometer、validity/fault。ERPMはVESC電気回転数であり、機械RPMは未実装なので mechanicalRpmValid=false。
- Type 66 WaypointSetPayload (276 bytes): request id、monotonic revision、action、1〜16点の緯度経度、reach radius、canonical CRC。
- Type 67 WaypointAckPayload (16 bytes): request/revision、accepted/rejected/duplicate、reason、active index/count、canonical CRC。既存 Type 23 は P1Capture START/STOP ACK のまま。

CoreS3 の型名・期待payload長・size static_assertを更新し、未知型扱いを防止した。

## PCデコーダ
python -m boat_eskf.min_shadow_log RUN.BIN --csv RUN.csv --txt RUN.TXT で Type 63〜65 を解析し、時刻逆行、型別sequence gap、NaN/Inf、出力範囲、version、trailing/transport sidecar診断を集計する。BINには復号済みフレームが入るため CRC/COBS/length の一次値はTXT診断を参照する。CSVは要求されたセンサ・制御・INA・VESC列を含む。実機BIN未投入のためBIN実データ件数は未確認。

## Waypoint運用
通信側 Web UI /waypoints と /api/waypoints を追加した。設定は STOP/DISARMED のみ受理し、RUNNING/E_STOP/状態不明は拒否する。座標・個数・revisionを検証してpending領域へ一括コピーし、制御側 Type 66 ACK が accepted の場合だけcanonical storeへ反映する。CRC、revision重複、範囲、空配列を拒否する。制御側は受理時にMIN内部をresetし、次のRUNNING開始で再初期化する。

## 検証
ホストC++ proposal_min_host PASS、Python unittest（14件）、compileall、PlatformIO の proposal_shadow_min、proposal_shadow_comm、m5stack-cores3 を実行済み。30分相当の100 Hzループはホストの決定的モデルで検証した。これらは実機書込み・COM通信・センサ性能を証明しない。