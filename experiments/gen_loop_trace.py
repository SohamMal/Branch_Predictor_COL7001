#!/usr/bin/env python3
"""Generate a synthetic bpsim trace of one loop branch.

A loop with trip count n produces (n-1) taken outcomes followed by 1 not-taken,
repeated `reps` times, all at the same PC.

Format (bpsim README): '# instructions: N' header, then '<pc hex> <0|1>' lines.

Usage:
    python gen_loop_trace.py --trip 4 --reps 1000 -o loop4.txt
    python gen_loop_trace.py --trip 4 6 8 --reps 1000 -o mixed.txt
        (several trip counts -> several loop branches at different PCs,
         run one after another: inner loop 1, then loop 2, ...)
"""
import argparse

INSNS_PER_BRANCH = 5  # only affects MPKI, not accuracy


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trip", type=int, nargs="+", required=True,
                    help="trip count(s) n >= 1; one loop branch per value")
    ap.add_argument("--reps", type=int, default=1000,
                    help="how many times each loop is executed in full")
    ap.add_argument("--pc", type=lambda s: int(s, 0), default=0x400100,
                    help="PC of the first loop branch (later ones +0x40 each)")
    ap.add_argument("-o", "--out", required=True)
    args = ap.parse_args()

    lines = []
    for r in range(args.reps):
        for k, n in enumerate(args.trip):
            pc = args.pc + 0x40 * k
            for i in range(n):
                taken = 1 if i < n - 1 else 0
                lines.append(f"{pc:x} {taken}\n")

    with open(args.out, "w") as f:
        f.write(f"# instructions: {len(lines) * INSNS_PER_BRANCH}\n")
        f.writelines(lines)

    print(f"wrote {len(lines)} branches to {args.out}")


if __name__ == "__main__":
    main()
