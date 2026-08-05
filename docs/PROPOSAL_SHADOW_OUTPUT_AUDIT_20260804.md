# SHADOW出力監査（2026-08-04）

- PCA9685 `setPulse` は `kPhysicalOutputCompileEnabled` と `kDryRunActuators` の二重条件下に限定。
- PCA9685 `allOff` は安全停止関数内でも同じコンパイル条件下。
- VESC `vescDuty` はコンパイル条件が偽なら即時returnし、UARTフレームを生成しない。
- `proposal_shadow_min` と benchmark/replay 環境は `ACTUATOR_OUTPUT_ENABLE=0` を強制し、設定ヘッダのstatic_assertで矛盾を拒否。
- 通信側XIAOはSD/GNSS/Web/UARTのみで、アクチュエータ出力を持たない。通信側にも同じstatic_assertとプロファイル定義を追加。

`rg`監査対象は `pca.setPulse`、`pca.allOff`、`vescSend`、`vescDuty`。実機通電・書込みによる出力確認は未実施。
