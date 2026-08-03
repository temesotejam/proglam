import json, os, time, threading, subprocess, urllib.parse
from datetime import datetime, timezone
import serial

root = os.environ.get("CAPTURE_ROOT", r"C:\Users\arika\Documents\PlatformIO\proglam\pc-tools\boat_eskf\captures\BNO_ESKF_STATIC60_20260803")
os.makedirs(root, exist_ok=True)
ports = {"core": "COM6", "xiao": "COM4"}
baud = 115200
raw_paths = {k: os.path.join(root, k + "_serial_raw.log") for k in ports}
stop_event = threading.Event()
lock = threading.Lock()
line_counts = {k: 0 for k in ports}
app_ready = {"core": False, "xiao": False}
ready_events = {"core": threading.Event(), "xiao": threading.Event()}
reader_errors = {k: [] for k in ports}
opened_mono = {}
serials = {}


def reader(name, ser):
    pending = b""
    with open(raw_paths[name], "ab", buffering=0) as f:
        failures = 0
        while not stop_event.is_set():
            try:
                data = ser.read(4096)
                failures = 0
            except Exception as e:
                failures += 1
                with lock:
                    reader_errors[name].append(repr(e))
                if failures > 80:
                    break
                time.sleep(0.10)
                continue
            if not data:
                continue
            f.write(data)
            pending += data
            while b"\n" in pending:
                line, pending = pending.split(b"\n", 1)
                text = line.decode("utf-8", "replace").rstrip("\r")
                with lock:
                    line_counts[name] += 1
                    if name == "core" and ("XIAO UART" in text or "RXDETAIL" in text or text.startswith("TRIAL_")):
                        app_ready[name] = True
                        ready_events[name].set()
                    if name == "xiao" and ("DIAG state=" in text or "TXDIAG" in text):
                        app_ready[name] = True
                        ready_events[name].set()
        if pending:
            with lock:
                line_counts[name] += 1


def curl_save(label, url, method="GET"):
    hp = os.path.join(root, label + ".headers")
    bp = os.path.join(root, label + ".body")
    ep = os.path.join(root, label + ".error")
    args = ["curl.exe", "--noproxy", "*", "--connect-timeout", "4", "--max-time", "10", "-sS", "-X", method, "-D", hp, "-o", bp, url]
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=15)
        if p.stderr:
            open(ep, "w", encoding="utf-8").write(p.stderr)
        status = None
        if os.path.exists(hp):
            for ln in open(hp, encoding="utf-8", errors="replace"):
                if ln.startswith("HTTP/"):
                    try:
                        status = int(ln.split()[1])
                    except Exception:
                        pass
                    break
        body = open(bp, "rb").read() if os.path.exists(bp) else b""
        return status, body, p.returncode
    except Exception as e:
        open(ep, "w", encoding="utf-8").write(repr(e) + "\n")
        return None, repr(e).encode(), -1


meta = {"run_label": "BNO_ESKF_STATIC60_20260803", "ports": ports, "baud": baud,
        "opened_utc": datetime.now(timezone.utc).isoformat(), "start_api": None, "end_utc": None}
for name, port in ports.items():
    s = serial.Serial(port, baudrate=baud, timeout=0.2, write_timeout=1, dsrdtr=False, rtscts=False)
    serials[name] = s
    opened_mono[name] = time.monotonic()
    threading.Thread(target=reader, args=(name, s), daemon=True).start()
open(os.path.join(root, "capture_open.json"), "w", encoding="utf-8").write(json.dumps(meta, indent=2, ensure_ascii=False))

ready_deadline = time.monotonic() + 20
while time.monotonic() < ready_deadline:
    if ready_events["core"].is_set() and ready_events["xiao"].is_set():
        break
    time.sleep(0.1)
ready_mono = time.monotonic()
if not (ready_events["core"].is_set() and ready_events["xiao"].is_set()):
    meta["status"] = "ABORT_APP_DIAG_NOT_READY"
    meta["line_counts"] = line_counts.copy()
    meta["app_ready"] = app_ready.copy()
    meta["reader_errors"] = reader_errors
    json.dump(meta, open(os.path.join(root, "capture_meta.json"), "w", encoding="utf-8"), indent=2, ensure_ascii=False)
    stop_event.set()
    for s in serials.values():
        try:
            s.close()
        except Exception:
            pass
    raise SystemExit(2)
open_elapsed = ready_mono - min(opened_mono.values())
if open_elapsed < 5:
    time.sleep(5 - open_elapsed)
with lock:
    meta["core_diag_lines_before_start"] = line_counts["core"]
    meta["xiao_diag_lines_before_start"] = line_counts["xiao"]
    meta["app_ready_before_start"] = app_ready.copy()
meta["prestart_wait_s"] = time.monotonic() - min(opened_mono.values())
json.dump(meta, open(os.path.join(root, "capture_ready.json"), "w", encoding="utf-8"), indent=2, ensure_ascii=False)

start_url = "http://192.168.4.1/api/log/start?duration_s=60&bno_capture=1&bno_log_enqueue=1&uart_rx_diag=dispatch"
start_epoch = time.time()
st, body, rc = curl_save("start_response", start_url, "POST")
meta["start_api"] = {"utc": datetime.now(timezone.utc).isoformat(), "epoch": start_epoch,
                     "url": start_url, "status": st, "curl_returncode": rc,
                     "body": body.decode("utf-8", "replace")}
json.dump(meta, open(os.path.join(root, "start_response.json"), "w", encoding="utf-8"), indent=2, ensure_ascii=False)

api_times = [5, 15, 30, 45, 60]
sent = set()
end_mono = time.monotonic() + 80
while time.monotonic() < end_mono:
    elapsed = time.time() - start_epoch
    for sec in api_times:
        if sec not in sent and elapsed >= sec:
            curl_save("eskf_%02ds" % sec, "http://192.168.4.1/api/eskf", "GET")
            sent.add(sec)
    time.sleep(0.1)

fs, fbody, frc = curl_save("files_after", "http://192.168.4.1/api/log/files", "GET")
try:
    files = json.loads(fbody.decode("utf-8", "replace"))
except Exception:
    files = {}
open(os.path.join(root, "files_after.json"), "w", encoding="utf-8").write(json.dumps(files, indent=2, ensure_ascii=False))
run_bin = run_txt = None
for item in files.get("files", []):
    n = item.get("name", "")
    if n.endswith(".BIN") and (run_bin is None or n > run_bin):
        run_bin = n
    if n.endswith(".TXT") and (run_txt is None or n > run_txt):
        run_txt = n
for n, lab in [(run_bin, "run_bin"), (run_txt, "run_txt")]:
    if n:
        curl_save(lab, "http://192.168.4.1/api/log/download?name=" + urllib.parse.quote(n), "GET")
        src = os.path.join(root, lab + ".body")
        if os.path.exists(src):
            open(os.path.join(root, n), "wb").write(open(src, "rb").read())
meta["files"] = {"status": fs, "curl_returncode": frc, "run_bin": run_bin, "run_txt": run_txt}
meta["line_counts_before_close"] = line_counts.copy()
meta["app_ready"] = app_ready.copy()
meta["reader_errors"] = reader_errors
meta["end_utc"] = datetime.now(timezone.utc).isoformat()
stop_event.set()
for s in serials.values():
    try:
        s.close()
    except Exception:
        pass
meta["line_counts_final"] = line_counts.copy()
json.dump(meta, open(os.path.join(root, "capture_meta.json"), "w", encoding="utf-8"), indent=2, ensure_ascii=False)
try:
    p = subprocess.run(["ping", "-n", "2", "192.168.4.1"], capture_output=True, text=True, timeout=5)
    open(os.path.join(root, "ping.txt"), "w", encoding="utf-8").write(p.stdout + p.stderr)
except Exception as e:
    open(os.path.join(root, "ping_error.txt"), "w", encoding="utf-8").write(repr(e))
print(json.dumps(meta, ensure_ascii=False), flush=True)
