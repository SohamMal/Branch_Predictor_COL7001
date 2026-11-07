#!/usr/bin/env python3
"""Plot a parameter sweep from bpsim result files.

Reads files named <prefix><value>.txt (e.g. results/local_L4.txt), each being
the stdout of `./bpsim -p <name> traces/*.gz`, and plots MPKI vs the swept
value: one line per trace plus a bold average line. Also prints a table.

Usage (from bpsim_student/bpsim):
    python ../../experiments/plot_sweep.py results/local_L  --param L
    python ../../experiments/plot_sweep.py results/gshare_H --param H
Output: <prefix>sweep.png next to the result files.
"""
import argparse
import glob
import re

import matplotlib.pyplot as plt


def parse(path):
    per_trace, avg, bits = {}, None, None
    with open(path) as f:
        for line in f:
            m = re.match(r"predictor:\s*\S+\s*\((\d+) bits\)", line)
            if m:
                bits = int(m.group(1))
                continue
            m = re.match(r"average MPKI:\s*([\d.]+)", line)
            if m:
                avg = float(m.group(1))
                continue
            parts = line.split()
            if len(parts) == 5 and parts[0] != "trace":
                per_trace[parts[0].split(".")[0]] = float(parts[4])
    return per_trace, avg, bits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("prefix", help="e.g. results/local_L")
    ap.add_argument("--param", default="value", help="axis label, e.g. L")
    args = ap.parse_args()

    runs = []
    for path in glob.glob(args.prefix + "*.txt"):
        m = re.fullmatch(re.escape(args.prefix) + r"(\d+)\.txt", path.replace("\\", "/"))
        if m:
            per, avg, bits = parse(path)
            if avg is None:
                print(f"skipping {path}: no results (empty file or failed run)")
                continue
            runs.append((int(m.group(1)), per, avg, bits))
    if not runs:
        raise SystemExit(f"no files matching {args.prefix}<number>.txt")
    runs.sort()

    xs = [r[0] for r in runs]
    traces = sorted(runs[0][1])

    print(f"{args.param:>4} {'bits':>7} {'avg MPKI':>9}  " + " ".join(f"{t:>11}" for t in traces))
    for x, per, avg, bits in runs:
        print(f"{x:>4} {bits or 0:>7} {avg:>9.3f}  " + " ".join(f"{per.get(t, float('nan')):>11.3f}" for t in traces))

    fig, ax = plt.subplots(figsize=(7, 4.5))
    for t in traces:
        ax.plot(xs, [r[1].get(t) for r in runs], marker="o", linewidth=1, alpha=0.7, label=t)
    ax.plot(xs, [r[2] for r in runs], marker="s", linewidth=2.5, color="black", label="average")
    ax.set_xlabel(args.param)
    ax.set_ylabel("MPKI (lower is better)")
    ax.set_xticks(xs)
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8)
    ax.set_title(f"MPKI vs {args.param}")
    fig.tight_layout()
    out = args.prefix + "sweep.png"
    fig.savefig(out, dpi=150)
    print(f"saved {out}")


if __name__ == "__main__":
    main()
