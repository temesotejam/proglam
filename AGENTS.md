# XIAO ESP32S3 project rules

These rules apply to every new or modified PlatformIO project for a Seeed Studio XIAO ESP32S3 in this workspace.

1. Provide a browser-accessible Web UI as part of the first usable firmware, unless the user explicitly says not to.
2. The default bring-up transport is a password-protected SoftAP. Put its SSID, password, HTTP port, and refresh interval in the project configuration header. Do not require an existing Wi-Fi router for first connection.
3. Provide a human-readable root page and a machine-readable JSON endpoint. The root page must show current data, units, validity, freshness/age, and fault or reinitialization state relevant to the experiment. For time-varying experiment data, it must also provide a browser-native time-series graph with a configurable refresh rate of at least 20 Hz unless the user explicitly chooses a lower rate.
4. Keep acquisition independent of page refresh: Web UI request handling must be non-blocking, bounded, and must not perform sensor I2C reads inside an HTTP handler.
5. Document the URL, connection method, credentials, API route, fields, units, and known limits in Japanese README text.
6. Build after a Web UI change and, when hardware is connected and in scope, upload the verified firmware before handoff.

