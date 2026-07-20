# proglam

Seeed Studio XIAO ESP32S3 を中心にした船体制御・センサー計測・SD 記録の実験用 PlatformIO プロジェクト集です。

## 主なプロジェクト

- `xiao-boat-control-integration`: BNO08X、VL53L5CX、INA226、PCA9685、VESC を統合した船体制御ファームウェア
- `xiao-boat-telemetry-sd-logger`: 統合機からの UART テレメトリーを microSD に記録するロガー
- `xiao_esp32s3_bno08x_bringup`: BNO08X の単体診断・Web UI 付き持ち込み確認
- `xiao-bno08x-vl53l5cx-sd-logger`、`xiao-ina226-sd-logger`、`xiao-pca9685-servo-sd-controller`、`xiao-vesc-*`: 統合前の個別検証プロジェクト

各プロジェクトは独立した PlatformIO プロジェクトです。対象プロジェクトのディレクトリで次を実行します。

```powershell
pio run
pio run -t upload
```

## Git 管理方針

- ソース、設定、README、変換・解析ツール、必要なサンプルデータを管理します。
- PlatformIO の `.pio/`、IDE 状態、Python キャッシュ、実機から取得した新規バイナリログは管理しません。
- `_reference_cores3_vl53l5cx_distance_map` と `_reference_vedderb_bldc` は外部の独立リポジトリのため、このリポジトリには含めません。

XIAO ESP32S3 向けに変更を加える際は、各プロジェクトの README とルートの `AGENTS.md` にあるWeb UI・SoftAP・JSON API・日本語ドキュメントの要件に従ってください。
