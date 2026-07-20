# Web UI

SoftAP `XIAO-BNO08X-SDLOG` / password `12345678` / `http://192.168.4.1/`.

- `POST /api/log/start`: 新規RUNログを開始要求
- `POST /api/log/stop`: キュー排出、flush、closeを要求
- `GET /api/latest`: BNO状態、SD状態、キュー、ドロップ、SDエラー、最新値、イベント別全区間Hz

ブラウザは2 Hz更新です。取得処理とSD書込みはHTTPハンドラから独立しています。
