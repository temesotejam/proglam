# 水上ボート現行機構対応・MIN実装監査（2026-08-04）

## 結論

現行ソースには、旧PCA9685 CH0の単一汎用サーボ試験経路とVESC UARTテレメトリ解析経路は存在するが、実機の4出力（left_front_wing、right_front_wing、rear_yaw、propulsion）へ確定した物理チャンネル接続は存在しない。したがって実機アクチュエータは有効化せず、4出力を同一計算式で生成するMIN SHADOW経路を制御側XIAOへ接続した。

## 実装済み・ホスト確認済み

- GNSS位置をローカルN/Eへ変換し、静的ウェイポイント列に対して距離・LOS目標方位・ラップ済みcourse errorを計算。
- IMUのroll/pitch/yawと各角速度、ToF中央値・傾き補正・LPFを入力にしたMIN計算。
- 前翼はcommon（height+pitch）とdifferential（roll）から左右を合成。
- 後翼は独立rear yaw出力として計算。
- 推進は単一出力とし、試験値は0.0、実機方法は未確定のため出力範囲も0.0。
- 全出力にneutral/min/max/sign/slewを適用し、prelimit値と最終値を保持。
- START/STOP/E-STOP/heartbeat timeout、センサstale、NaN/Infで安全値へ遷移。
- ControlOutput（Type 62）で4出力、prelimit、u_height/u_pitch/u_roll、目標方位、course error、WP距離、状態、停止理由、SHADOW/validを制御側から送信。
- CoreS3はType 62の長さ検証・名称認識を追加。通信側XIAOは既存の汎用フレームSD保存経路でRAW制御結果を保存する。

## 未実装・実機未確認

- 左右前翼、後翼yaw、推進のPCA/VESC実物チャンネル・中立・符号・可動範囲。
- Webからのウェイポイント登録を制御側へ送る専用コマンド。現段階は制御側に静的MINルートを接続。
- INA226実測取得は既存設定が無効（kEnableIna226=false）。配線名と有効化条件を確認後に別途実装。
- VESCはテレメトリのみ。機械RPMは極対数未確定のため算出しない。
- 実機書込み、配線確認、航走試験。

## ビルド・テスト

- proposal_min_host: PASS
- Python unittest: 11 tests PASS
- 制御XIAO proposal_shadow_min: SUCCESS（RAM 63.6%, Flash 17.5%）
- 制御XIAO通常環境: SUCCESS（RAM 63.6%, Flash 17.3%）
- 通信側XIAO Senseプロジェクト proposal_shadow_comm: SUCCESS（RAM 58.1%, Flash 26.7%）
- 仮通信側CoreS3 m5stack-cores3: SUCCESS（RAM 51.3%, Flash 15.8%）
- 実機試験: 未実施

## 安全判定

ACTUATOR_OUTPUT_ENABLE=0、SHADOW_CONTROL_ENABLE=1のままで、PCA/VESCへの実出力は行わない。物理マッピングが確定するまで、旧単一サーボ名やrudder/elevonを最終出力として扱わない。
