import sys, random

# usage: make_tp53_mut.py <ref.bin> <out.bin> <pct>
ref_path, out_path, pct = sys.argv[1], sys.argv[2], float(sys.argv[3])

with open(ref_path, "rb") as f:
    ref = f.read()

N = len(ref)
n_mut = int(round(N * pct / 100.0))

b = bytearray(ref)
rng = random.Random(12345)          # same seed for reproducibility
positions = rng.sample(range(N), n_mut)
for i in positions:
    b[i] = (b[i] + 1) % 4

with open(out_path, "wb") as f:
    f.write(bytes(b))

print(f"{out_path}: N={N}  pct={pct}%  n_mut={n_mut}")