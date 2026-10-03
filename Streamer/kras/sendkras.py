#!/usr/bin/env python3
import socket, sys, random, time

if len(sys.argv) < 3:
    print("usage: send_dna.py <PI_IP> <healthy|mutX|allA> [N]")
    sys.exit(1)

PI_IP = sys.argv[1]
MODE  = sys.argv[2]
N     = int(sys.argv[3]) if len(sys.argv) > 3 else 5_000_000
CHUNK = 65536

random.seed(42)
base = bytes(random.randint(0, 3) for _ in range(N))

if MODE == "healthy":
    ref = base
    sam = base                            # identical

elif MODE.startswith("mut"):
    pct = float(MODE[3:])
    b = bytearray(base)
    n_mut = int(round(N * pct / 100.0))
    rng = random.Random(12345)
    positions = rng.sample(range(N), n_mut)   # random, no repeats
    for i in positions:
        b[i] = (b[i] + 1) % 4
    ref = base
    sam = bytes(b)
    print(f"mode={MODE}  mutation_rate={pct}%  n_mut={n_mut}  positions=random")

elif MODE == "allA":
    ref = base
    sam = bytes([0]) * N                  # fully mismatched against random ref

elif MODE.endswith(".bin"):
    with open(MODE, "rb") as f:
        ref = f.read()
    # Reference is the unmutated KRAS tiled to the same length
    with open("kras_reference.bin", "rb") as f:
        ref_seq = f.read()
    sam = ref
    ref = ref_seq

else:
    print("unsupported mode"); sys.exit(1)

def connect_with_retry(ip, port, max_wait=60.0):
    deadline = time.time() + max_wait
    while time.time() < deadline:
        try:
            return socket.create_connection((ip, port), timeout=5)
        except (ConnectionRefusedError, ConnectionResetError, OSError):
            time.sleep(0.3)
    raise RuntimeError(f"could not connect to {ip}:{port}")

s = connect_with_retry(PI_IP, 9999)
for i in range(0, len(ref), CHUNK):
    r = ref[i:i+CHUNK]
    m = sam[i:i+CHUNK]
    assert len(r) == len(m)
    s.sendall(len(r).to_bytes(4, "little"))
    s.sendall(r)
    s.sendall(m)
s.sendall((0).to_bytes(4, "little"))
s.close()
print(f"sent {len(ref)} base pairs in mode={MODE}")