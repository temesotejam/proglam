# 制御側XIAO ESKF SHADOW

制御側XIAO ESP32S3でBNO08X、VL53L5CX、15誤差状態ESKFを実行します。現在はCoreS3暫定通信ブリッジからGNSSを受信します。

- 名目状態: NED位置・速度、body FRD → NEDクォータニオン、加速度/ジャイロbias
- 誤差状態: `[δp, δv, δθ, δba, δbg]` の15状態
- IMU予測、15×15共分散伝播、Joseph形式更新、NISゲート、GNSS重複排除、ToF複数ゾーン観測を実装
- `EstimatedState` は従来baselineとして残し、`EskfState/EskfInnovation/EskfHealth` を追加

## 安全固定値

`kDryRunActuators=true`、`kShadowOnly=true`、`kActuatorOutputEnabled=false`、`kEnableIna226=false`、`kSecondaryBnoEnabled=false` です。起動状態はDISARMEDで、PCA9685はFull OFF、VESCはDuty 0です。ESKFの状態を物理出力やARMへ接続していません。

BNO取付変換とToF取付位置は未較正の恒等・原点仮定です。その間も推定は実行しますが、mount healthはDEGRADEDです。