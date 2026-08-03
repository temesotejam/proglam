# 別PCへの引継ぎと再現手順

最終更新: 2026-07-27

## この文書の目的

別PCの作業者が、現在のソース、実装済み範囲、検証済み範囲、未確定事項を混同せずに引き継ぐための文書です。成功を意味する記録と、まだ実機で確認していない項目を明確に分けます。

## 取得するGitHub状態

現時点の正本となる作業ブランチは `agent/bno-int-telemetry-handoff` です。`main` だけを取得しても、今回の仮接続・校正ログ・影推定の変更は含まれません。

```powershell
git clone https://github.com/temesotejam/proglam.git
cd proglam
git switch agent/bno-int-telemetry-handoff
git pull
```

引継ぎ時の確定コミットは `1f55585 Add provisional calibration integration` です。取得後は `git log -1 --oneline` と `git status -sb` で、このコミットを基準に作業を開始してください。

## 別PCでも再現できるもの

- 両XIAO向けのファームウェアソース、PlatformIO設定、共有UARTプロトコル。
- 通信側SoftAP/Web UI、JSON API、SDへの校正ログ開始・停止処理。
- 制御側IMUスナップショットと通信側の二重IMU比較、および品質ゲート付きの仮（shadow）姿勢補正。
- GNSS、ToF、仮想VESC、3翼の入力を一つの仮システム表示へまとめる処理。
- `DRY_RUN` / `DISARMED` を維持した実出力ゼロの安全状態。
- RUN0020をBOAT24合格の通信・ロギング基準として扱う判断、実装履歴、ビルド・書込み・Web API確認の記録。

## 別PCで追加で必要なもの

ソースを取得するだけでは実機動作は再現されません。次を別PCに用意し、接続状態を確認します。

1. Git、PlatformIO Core（またはPlatformIO IDE）、ESP32用USBシリアルドライバ。
2. 制御側・通信側のXIAO ESP32S3、同一のセンサ・UART・SD配線、電源、microSDカード。
3. 各プロジェクトをビルドして、それぞれ認識されたCOMポートへ書き込む作業。
4. 通信側SoftAP `XIAO-BOAT-TELEMETRY`（パスワード `12345678`）への接続。通信側のWeb UIは `http://192.168.4.1/`、APIは `http://192.168.4.1/api/...` です。

COM番号はPCやUSB接続順で変わるため固定値として扱いません。直近の実機識別の記録は、通信側 MAC `E0:72:A1:FC:08:D0`、制御側 MAC `34:85:18:AB:FA:90` です。

## 現在の検証済み到達点

- RUN0020では、内部BNO08X、基板間UART、GNSS受信、ToF、SD記録についてBOAT24基準を通過しています。UARTは921600 bpsでNAV送信802回／結果受信802回、CRCエラー0、ToFは1,800フレームでI2Cエラー0、SDの書込みエラーとキュー落ちは0でした。
- 通信側の仮システム表示は実機で確認済みです。二重IMUの品質ゲートとToF表示は動作し、出力は常にゼロ、モードは `DISARMED` です。
- 校正セッションのWeb APIとSDログ開始・停止の実装、通信側への書込み、起動後のAPI応答は確認済みです。

## 未確定・未実施（成功として扱わない）

- 機体へ固定後に行う正式な6面静止、各軸回転、磁気、時刻オフセット、ToF、舵ジオメトリ、VESCテレメトリの校正。
- 校正値を使った正式な取付け姿勢の検証、ESKFの正式有効化、航法結果の受入判定。
- GNSSの有効Fixを用いた現地検証。直近の仮表示ではGNSS Fixは新鮮な有効状態ではありませんでした。
- PCA9685、VESC、翼・推進器への実出力。現ファームウェアは意図的に `DRY_RUN` / `DISARMED` で、実出力はゼロです。
- 物理取付け後の再現試験。USBを固定できない机上校正は、正式校正の代替にはしません。

## 引継ぎ後の安全な順序

1. GitHubから上記ブランチとコミットを取得し、作業ツリーが意図せず変更されていないことを確認する。
2. 両プロジェクトをビルドする。通信側と制御側を別々に書き込む前に、COMポートと基板識別を確認する。
3. 通信側SoftAPと `/api/manual`、`/api/provisional-system`、`/api/calibration` を確認し、SD ready、リンク状態、`DRY_RUN=true`、`DISARMED` を確認する。
4. 機体へ固定後にのみ、校正画面から各校正を別RUNとして記録する。姿勢を再現できない状態では正式校正を開始しない。
5. 校正ログのPC再生・検証を終えるまで、ESKFを制御へ接続せず、実出力を有効化しない。

## GitHubに含めないもの

ローカルの `.tmp_bench*` は一時解析データであり、再現に必要なソースや判断記録ではないためGitHubへ含めていません。必要な恒久的な試験根拠は `docs/WORK_LOG.md`、`docs/VALIDATION_EVIDENCE.md`、関連する設計・解析文書に保存します。
