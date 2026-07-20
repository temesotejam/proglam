#!/usr/bin/env python3
"""Convert INA226_BIN_V1 files.  Specify --out; no derived files are written beside an SD log."""
import argparse, csv, os, struct, sys
HEADER_MAGIC = b"ILOG"
RECORD_MAGIC = b"IRCD"
RECORD_SIZE = 160
TYPES = {1: "INA_CONFIG", 2: "INA_MEASUREMENT", 3: "INA_STATUS", 4: "SYSTEM_STATUS", 9: "LOG_END"}

def s32(v): return v if v < 0x80000000 else v - 0x100000000

def open_writer(path, fields):
    f = open(path, "w", newline="", encoding="utf-8")
    w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
    w.writeheader()
    return f, w

def main():
    ap = argparse.ArgumentParser(description="INA226_BIN_V1 to CSV converter")
    ap.add_argument("bin", help="/INALOG/RUNxxxx.BIN copied from the SD card")
    ap.add_argument("--out", required=True, help="output folder on the PC (must not be the SD card root)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    stem = os.path.splitext(os.path.basename(a.bin))[0]
    mf, meas = open_writer(os.path.join(a.out, stem + "_ina_measurement.csv"), [
        "record_sequence", "rx_timestamp_us", "sample_sequence", "valid", "raw_shunt", "raw_bus", "raw_current", "raw_power", "mask_enable", "shunt_v", "bus_v", "current_shunt_a", "current_register_a", "power_register_w", "power_calculated_w", "i2c_duration_us"])
    cf, conf = open_writer(os.path.join(a.out, stem + "_ina_config.csv"), ["record_sequence", "rx_timestamp_us", "sensor_online", "address", "i2c_hz", "manufacturer_id", "die_id", "config", "calibration", "shunt_ohm", "current_lsb_a", "power_lsb_w"])
    sf, status = open_writer(os.path.join(a.out, stem + "_ina_status.csv"), ["record_sequence", "rx_timestamp_us", "sensor_online", "samples", "invalid_samples", "i2c_errors", "initialization_attempts", "max_i2c_us", "max_gap_us", "queue_used", "log_drops", "sd_write_errors", "last_good_age_ms"])
    counts = {name: 0 for name in TYPES.values()}
    with open(a.bin, "rb") as f:
        head = f.read(256)
        if len(head) < 20 or head[:4] != HEADER_MAGIC:
            raise SystemExit("not an INA226_BIN_V1 file (ILOG header missing)")
        version, header_bytes, record_bytes = struct.unpack_from(">HHH", head, 4)
        if record_bytes != RECORD_SIZE:
            raise SystemExit("unsupported record size %d" % record_bytes)
        f.seek(header_bytes)
        while True:
            r = f.read(record_bytes)
            if not r: break
            if len(r) != record_bytes:
                print("warning: truncated final record ignored", file=sys.stderr); break
            magic, ver, typ, flags, size, sequence, rx_us, request_us, round_trip_us, packet_seq = struct.unpack_from(">4sHBBHIQQII", r, 0)
            if magic != RECORD_MAGIC or ver != version or size != record_bytes:
                print("warning: invalid record at byte %d ignored" % (f.tell() - record_bytes), file=sys.stderr); continue
            p = r[38:156]
            name = TYPES.get(typ, "UNKNOWN")
            counts[name] = counts.get(name, 0) + 1
            if typ == 1:
                address, hz, mid, did, cfg, cal = struct.unpack_from(">6I", p, 0)
                shunt, current_lsb, power_lsb = struct.unpack_from("<3f", p, 24)
                conf.writerow(dict(record_sequence=sequence, rx_timestamp_us=rx_us, sensor_online=bool(flags & 1), address="0x%02X" % address, i2c_hz=hz, manufacturer_id="0x%04X" % mid, die_id="0x%04X" % did, config="0x%04X" % cfg, calibration="0x%04X" % cal, shunt_ohm=shunt, current_lsb_a=current_lsb, power_lsb_w=power_lsb))
            elif typ == 2:
                raw_shunt, raw_bus, raw_current, raw_power, mask = struct.unpack_from(">iIiII", p, 0)
                shunt, bus, ishunt, ireg, preg, pcalc = struct.unpack_from("<6f", p, 20)
                i2cus = struct.unpack_from(">I", p, 44)[0]
                meas.writerow(dict(record_sequence=sequence, rx_timestamp_us=rx_us, sample_sequence=packet_seq, valid=bool(flags & 1), raw_shunt=raw_shunt, raw_bus=raw_bus, raw_current=raw_current, raw_power=raw_power, mask_enable="0x%04X" % mask, shunt_v=shunt, bus_v=bus, current_shunt_a=ishunt, current_register_a=ireg, power_register_w=preg, power_calculated_w=pcalc, i2c_duration_us=i2cus))
            elif typ == 3:
                values = struct.unpack_from(">10I", p, 0)
                status.writerow(dict(record_sequence=sequence, rx_timestamp_us=rx_us, sensor_online=bool(flags & 1), samples=values[0], invalid_samples=values[1], i2c_errors=values[2], initialization_attempts=values[3], max_i2c_us=values[4], max_gap_us=values[5], queue_used=values[6], log_drops=values[7], sd_write_errors=values[8], last_good_age_ms=values[9]))
    for fh in (mf, cf, sf): fh.close()
    print("format_version=%d header_bytes=%d records=%s" % (version, header_bytes, counts))

if __name__ == "__main__": main()