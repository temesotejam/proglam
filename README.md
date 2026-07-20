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

## 作業の引継ぎ

会話が切り替わっても作業状況を復元できるよう、次のファイルを正本として管理します。新しい作業を始める際は、この順で確認してください。

1. [`docs/PROJECT_CONTEXT.md`](docs/PROJECT_CONTEXT.md): 目的、アーキテクチャ、確定済み制約
2. [`docs/WORK_PLAN.md`](docs/WORK_PLAN.md): 段階的な実施計画と現在地
3. [`docs/WORK_LOG.md`](docs/WORK_LOG.md): 実施内容、結果、判断の時系列

統合コードの現在の実装済み範囲と未実装範囲は、[`docs/INTEGRATION_GAP_REVIEW.md`](docs/INTEGRATION_GAP_REVIEW.md) に記録しています。

実機成功の根拠と未確認事項は、[`docs/VALIDATION_EVIDENCE.md`](docs/VALIDATION_EVIDENCE.md) に記録しています。

通信側UART→SD保存の実機BIN解析結果は、[`docs/BOATLOG_ANALYSIS.md`](docs/BOATLOG_ANALYSIS.md) に記録しています。

2台・全センサ・記録・監視・安全停止を先に接続する方針と実装順序は、[`docs/SYSTEM_VERTICAL_SLICE_PLAN.md`](docs/SYSTEM_VERTICAL_SLICE_PLAN.md) に記録しています。

作業を終えるときは、計画の状態・次の作業・検証結果を必ず更新します。

## Git 管理方針

- ソース、設定、README、変換・解析ツール、必要なサンプルデータを管理します。
- PlatformIO の `.pio/`、IDE 状態、Python キャッシュ、実機から取得した新規バイナリログは管理しません。
- `_reference_cores3_vl53l5cx_distance_map` と `_reference_vedderb_bldc` は外部の独立リポジトリのため、このリポジトリには含めません。

XIAO ESP32S3 向けに変更を加える際は、各プロジェクトの README とルートの `AGENTS.md` にあるWeb UI・SoftAP・JSON API・日本語ドキュメントの要件に従ってください。
