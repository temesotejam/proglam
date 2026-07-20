# XIAO ESP32S3 project rules

These rules apply to every new or modified PlatformIO project for a Seeed Studio XIAO ESP32S3 in this workspace.

## 作業引継ぎ

コンテキスト切替後を含め、このワークスペースで作業を始める前に、次の順で必ず確認すること。

1. `docs/PROJECT_CONTEXT.md` — 目的、役割分担、確定済みの制約
2. `docs/WORK_PLAN.md` — 作業順序、現在の段階、次に行う作業
3. `docs/WORK_LOG.md` — 判断・実施・検証の時系列記録

コード、設定、実機検証、または重要な判断を変更した場合は、同じ作業で `WORK_PLAN.md` の状態と `WORK_LOG.md` を更新する。未検証の内容を完了・成功として記録してはならない。

プロジェクト固有の役割分担が `PROJECT_CONTEXT.md` に明記され、ここにある一般ルールと矛盾する場合は、ユーザーが明示したその役割分担を優先する。

1. Provide a browser-accessible Web UI as part of the first usable firmware, unless the user explicitly says not to.
2. The default bring-up transport is a password-protected SoftAP. Put its SSID, password, HTTP port, and refresh interval in the project configuration header. Do not require an existing Wi-Fi router for first connection.
3. Provide a human-readable root page and a machine-readable JSON endpoint. The root page must show current data, units, validity, freshness/age, and fault or reinitialization state relevant to the experiment. For time-varying experiment data, it must also provide a browser-native time-series graph with a configurable refresh rate of at least 20 Hz unless the user explicitly chooses a lower rate.
4. Keep acquisition independent of page refresh: Web UI request handling must be non-blocking, bounded, and must not perform sensor I2C reads inside an HTTP handler.
5. Document the URL, connection method, credentials, API route, fields, units, and known limits in Japanese README text.
6. Build after a Web UI change and, when hardware is connected and in scope, upload the verified firmware before handoff.
