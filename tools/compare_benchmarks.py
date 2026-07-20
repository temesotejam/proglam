#!/usr/bin/env python3
"""Compare two BENCH/BOATLOG runs using the compatible decoder."""
from __future__ import annotations
import argparse, csv, pathlib, tempfile
from analyze_benchmark import analyze
def main():
    p=argparse.ArgumentParser(); p.add_argument("run_10cm",type=pathlib.Path); p.add_argument("run_1m",type=pathlib.Path); p.add_argument("--output",type=pathlib.Path,required=True); a=p.parse_args(); a.output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as t:
        n10,d10=analyze(a.run_10cm,pathlib.Path(t)/"ten"); n1,d1=analyze(a.run_1m,pathlib.Path(t)/"one")
    rows=[{"metric":"records","10cm":n10,"1m":n1},{"metric":"duration_s","10cm":f"{d10:.3f}","1m":f"{d1:.3f}"}]
    with (a.output/"comparison.csv").open("w",newline="",encoding="utf-8") as f: w=csv.DictWriter(f,fieldnames=["metric","10cm","1m"]);w.writeheader();w.writerows(rows)
    (a.output/"comparison.md").write_text("# 10 cm / 1 m比較\n\n`comparison.csv`は構造・件数比較です。ToF取得率、I2Cエラー、読取り時間は各RUNのphase_resultを実機で取得後に比較します。\n",encoding="utf-8")
if __name__ == "__main__": main()
