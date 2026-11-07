#!/usr/bin/env python3
"""Validation traces for bpsim: same branch kinds as tools/gen_trace.py, but
built differently, to check whether parameters tuned on traces/ generalise.

Differences from tools/gen_trace.py:
  * correlation distance: a correlated branch copies the branch d positions
    earlier (the public generator always uses d = 1)
  * skewed hotness: code is split into blocks; each step runs one block,
    picked with Zipf weights, so a few blocks are hot (public traces run every
    branch equally often, round-robin)
  * different loop trip counts and bias probabilities

Usage: python gen_validation_traces.py <out_dir> [--branches N] [--seed S]
"""
import argparse
import gzip
import os
import random

LOOP_TRIPS = [2, 6, 7, 9, 12, 20, 24, 64]
BIAS_P = [0.02, 0.08, 0.15, 0.85, 0.92, 0.98]


class Branch:
    def __init__(self, pc, kind, rng, **kw):
        self.pc, self.kind, self.rng, self.kw = pc, kind, rng, kw
        self.count = 0
        self.last = False

    def outcome(self):
        k = self.kind
        if k == "loop":
            n = self.kw["trip"]
            t = (self.count % n) != n - 1
        elif k == "alternate":
            t = self.count % 2 == 0
        elif k == "biased":
            t = self.rng.random() < self.kw["p"]
        elif k == "correlated":
            src = self.kw["src"]
            t = src.last if not self.kw["invert"] else not src.last
        else:  # random
            t = self.rng.random() < 0.5
        self.count += 1
        self.last = t
        return t


def make_blocks(mix, n_static, block_size, dists, rng):
    """n_static branches split into blocks; correlated branches copy the branch
    d earlier in the same block, d drawn from `dists`."""
    kinds, weights = list(mix), list(mix.values())
    used_pcs, blocks, block = set(), [], []
    for _ in range(n_static):
        while True:
            pc = 0x400000 + 4 * rng.randrange(1, 1 << 22)
            if pc not in used_pcs:
                used_pcs.add(pc)
                break
        kind = rng.choices(kinds, weights)[0]
        kw = {}
        if kind == "loop":
            kw["trip"] = rng.choice(LOOP_TRIPS)
        elif kind == "biased":
            kw["p"] = rng.choice(BIAS_P)
        elif kind == "correlated":
            d = rng.choice(dists)
            if len(block) >= d:
                kw["src"] = block[-d]
                kw["invert"] = rng.random() < 0.5
            else:
                kind, kw = "biased", {"p": rng.choice(BIAS_P)}
        block.append(Branch(pc, kind, rng, **kw))
        if len(block) == block_size:
            blocks.append(block)
            block = []
    if block:
        blocks.append(block)
    return blocks


def write_trace(path, blocks, n_dyn, zipf_s, rng):
    order = list(range(len(blocks)))
    rng.shuffle(order)
    weights = [0.0] * len(blocks)
    for rank, b in enumerate(order, start=1):
        weights[b] = 1.0 / rank ** zipf_s
    instructions, lines = 0, []
    while len(lines) < n_dyn:
        blk = blocks[rng.choices(range(len(blocks)), weights)[0]]
        for b in blk:
            lines.append(f"{b.pc:x} {int(b.outcome())}\n")
            instructions += rng.randint(2, 8)
            if len(lines) >= n_dyn:
                break
    with gzip.open(path, "wt") as f:
        f.write(f"# instructions: {instructions}\n")
        f.writelines(lines)


MIXED = {"loop": 3, "biased": 4, "correlated": 2, "alternate": 1, "random": 1}
CORR = {"correlated": 2, "random": 1}

TRACES = {
    # name: (mix, static branches, block size, correlation distances, zipf s)
    "v_corr_d1":  (CORR, 64, 16, [1], 0.0),
    "v_corr_d4":  (CORR, 64, 16, [4], 0.0),
    "v_corr_d8":  (CORR, 64, 16, [8], 0.0),
    "v_corr_d12": (CORR, 64, 16, [12], 0.0),
    "v_mix_small":  (MIXED, 500, 20, list(range(1, 13)), 1.0),
    "v_mix_medium": (MIXED, 2000, 20, list(range(1, 13)), 1.0),
    "v_mix_large":  (MIXED, 8000, 20, list(range(1, 13)), 1.0),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("--branches", type=int, default=1_000_000)
    ap.add_argument("--seed", type=int, default=424242)
    a = ap.parse_args()
    os.makedirs(a.out_dir, exist_ok=True)
    for name, (mix, n_static, bsize, dists, zipf_s) in TRACES.items():
        rng = random.Random(f"{a.seed}-{name}")
        blocks = make_blocks(mix, n_static, bsize, dists, rng)
        path = os.path.join(a.out_dir, f"{name}.trace.gz")
        write_trace(path, blocks, a.branches, zipf_s, rng)
        print("wrote", path)


if __name__ == "__main__":
    main()
