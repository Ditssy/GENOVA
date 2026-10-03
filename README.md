<div align="center">

# 🧬 GENOVA

### **GEN**omics × Computer Architecture × Edge Computing

*What if the CPU processing the DNA could tell us something about the DNA?*

![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi-C51A4A?logo=raspberrypi&logoColor=white)
![Language](https://img.shields.io/badge/Kernel-C%2FC%2B%2B-00599C?logo=c&logoColor=white)
![Sender](https://img.shields.io/badge/Client-Python%203-3776AB?logo=python&logoColor=white)
![Telemetry](https://img.shields.io/badge/Telemetry-Linux%20perf__event-FCC624?logo=linux&logoColor=black)
![Status](https://img.shields.io/badge/Status-Research%20Prototype-orange)

</div>

---

## 📌 Table of Contents

1. [Motivation](#-motivation)
2. [What Problem Does GENOVA Solve?](#-what-problem-does-genova-solve)
3. [The Idea](#-the-idea)
4. [Project Workflow](#-project-workflow)
5. [System Architecture](#-system-architecture)
6. [Project Architecture (Repository)](#-project-architecture-repository)
7. [The Deterministic Kernel (`genova_pi.c`)](#-the-deterministic-kernel-genova_pic)
8. [The DNA Sender (`send_dna.py`)](#-the-dna-sender-send_dnapy)
9. [PMU Signature & Hardware Feature Vector](#-pmu-signature--hardware-feature-vector)
10. [Wire Protocol](#-wire-protocol)
11. [Quick Start](#-quick-start)
12. [Experiment: Controlled Mutation Sweep (KRAS and TP53)](#-experiment-controlled-mutation-sweep-kras-and-tp53)
13. [What GENOVA Is — and Is Not](#-what-genova-is--and-is-not)
14. [Roadmap](#-roadmap)
15. [Limitations & Honest Notes](#-limitations--honest-notes)

---

## 🎯 Motivation

Cancer is not only a disease of biology — it is also a **race against time**.

Some cancer-associated genomic changes can be very small, yet identifying meaningful changes early can be extremely important: the earlier an abnormality is found, the more opportunity there may be for further investigation and treatment.

Genomic analysis today is computationally intensive and involves multiple stages of testing. This raised our question:

> **Can we use the computer itself to extract another signal from genomic data?**

---

## ❓ What Problem Does GENOVA Solve?

### The problem

1. **Early detection is a race against time.** Small cancer-associated genomic changes are easy to miss, and every extra stage of testing adds delay before an abnormality is investigated.
2. **Genomic analysis is heavy.** It needs computationally intensive processing and several stages of testing, often in centralized, resource-rich environments.
3. **A free signal is thrown away.** When a CPU processes DNA, its hardware behaviour (cache misses, branch mispredictions, IPC) is measured by the PMU but normally ignored. Only the software output is used.
4. **There is no lightweight first-look layer.** Researchers lack a fast, cheap computational check that can run at the edge before committing to deeper analysis.

### How GENOVA addresses it

| Problem | GENOVA's approach |
|---------|-------------------|
| Delay before an abnormality is noticed | A fast hardware-signature check that could act as a screening or triage layer |
| Unused hardware information | Turns PMU counters into a **hardware feature vector** compared with a baseline |
| Dependence on heavy infrastructure | Runs on a low-cost **Raspberry Pi** (edge computing) |
| Unknown whether input changes affect the CPU measurably | Shows, with a controlled deterministic kernel, that **changing the genomic input changes CPU behaviour** |

### What has been shown so far

> In the controlled KRAS experiment, deliberately introduced sequence changes produced **measurable changes in the CPU's microarchitectural behaviour** under a deterministic workload.

That is the proof of concept: a genomic change can be seen as a hardware signal.

### What is not solved yet

- It does **not** diagnose cancer, and no cache-miss value "means cancer".
- It has **not** yet been tested on real tumour and normal datasets. That is the next step.
- Whether the hardware signature adds information beyond a plain software comparison is still an open research question.

---

## 💡 The Idea

Instead of treating the CPU as *just a machine that processes DNA*, GENOVA observes **how the CPU behaves while processing DNA**.

1. Genomic data is passed through a **controlled, deterministic genomic-processing kernel** written in C.
2. The workload runs on a **Raspberry Pi CPU** (pinned to a single core).
3. The CPU's **Performance Monitoring Unit (PMU)** measures low-level hardware behaviour:
   cache misses · branch mispredictions · instructions · CPU cycles · IPC
4. These measurements become a **hardware feature vector**, compared against a **reference baseline**.

In our controlled **KRAS** experiment we start from a reference sequence and deliberately introduce controlled sequence changes. As the input changes, the CPU's **microarchitectural behaviour changes with it**.

> ⚠️ This does **not** mean a particular cache-miss value means cancer.
> It demonstrates something more fundamental: **a change in genomic input can produce a measurable change in CPU behaviour under a controlled workload.**

---

## 🔄 Project Workflow

```text
+---------------------------+
|  1. GENOMIC INPUT         |
|  KRAS reference/TP53      |
|  REFERENCE                |
+-------------+-------------+
              |
              v
+---------------------------+
|  2. ENCODE                |
|  A,C,G,T  ->  0,1,2,3     |
+-------------+-------------+
              |
              v
+---------------------------+
|  3. STREAM OVER TCP       |
|  send*.py -> port 9999    |
+-------------+-------------+
              |
              v
+---------------------------+
|  4. DETERMINISTIC KERNEL  |
|  genova_pi.c (C)          |
|  Raspberry Pi, core 2     |
+-------------+-------------+
              |
              v
+---------------------------+
|  5. PMU MEASUREMENT       |
|  cache misses             |
|  branch mispredictions    |
|  instructions, cycles     |
+-------------+-------------+
              |
              v
+---------------------------+
|  6. HARDWARE FEATURE      |
| bm_per_kb, Cluster %      |
| detection with the help of|
| Modality function         |
+-------------+-------------+
              |
              v
+---------------------------+
|  7. COMPARE WITH          |
|  REFERENCE BASELINE       |
+------+------------+-------+
       |            |
       v            v
+------------+  +-----------------+
| Deviation  |  | Within baseline |
| FLAG FOR   |  | CONSISTENT WITH |
| FURTHER    |  | REFERENCE       |
| INVESTIGATE|  |                 |
+------------+  +-----------------+
```

### Step-by-step
**VERY VERY IMPORTANT: CORE ISOLATION, CLOCK SPEED FIXATION, THERMAL THROTTLING, BUFFER BOTTLENECK MANAGEMENT**
| # | Stage | What happens |
|---|-------|--------------|
| 1 | **Prepare** | Build reference and sample sequences (`makekras.py`, `maketp53.py`, `make_classes.py`) and save as `.bin` files |
| 2 | **Stream** | `sendkras.py` / `sendtp53.py` send reference + sample chunk pairs to the Pi over TCP |
| 3 | **Process** | `genova_pi.c` runs a pairwise reference-vs-sample comparator on core 2 |
| 4 | **Measure** | PMU counters are enabled for the whole session and read at the end |
| 5 | **Featurize** | Raw counts become normalized features (IPC, misses per 1000 bases, mismatch %) |
| 6 | **Compare** | Feature Factor is compared against the baseline to get a deviation signature  after being increased by the clustering % multiplied |

---

## 🏗️ System Architecture

```text
+====================================================================+
|  HOST MACHINE  (Sender)                                            |
|                                                                    |
|   kras_ref.fasta / tp53_RefSeqGene.fasta                           |
|        |                                                           |
|        v                                                           |
|   makekras.py / maketp53.py / make_classes.py                      |
|        |                                                           |
|        v                                                           |
|   .bin datasets                                                    |
|   (reference + mutation levels 0.0005 ... 25.2 %)                  |
|        |                                                           |
|        v                                                           |
|   sendkras.py / sendtp53.py   (chunked TCP client)                 |
+========================================+===========================+
                                         |
                                         |  TCP port 9999
                                         |  [4-byte length][ref chunk][sample chunk]
                                         v
+====================================================================+
|  RASPBERRY PI  (Edge Node)                                         |
|                                                                    |
|  +--------------------- USER SPACE ----------------------------+   |
|  |                                                             |   |
|  |   Socket server (recv loop)                                 |   |
|  |        |                                                    |   |
|  |        v                                                    |   |
|  |   Aligned buffers: ref_buf, sam_buf (64-byte, 1 MiB)        |   |
|  |        |                                                    |   |
|  |        v                                                    |   |
|  |   process_pairwise()  <-- deterministic kernel              |   |
|  |        |                                                    |   |
|  |        |  on every mismatch                                 |   |
|  |        v                                                    |   |
|  |   chaos()  (64 branches + 8 memory reads, 16 MiB buffer)    |   |
|  |                                                             |   |
|  +-------------------------------------------------------------+   |
|                              |                                     |
|                              v                                     |
|  +----------------- KERNEL / HARDWARE -------------------------+   |
|  |                                                             |   |
|  |   ARM PMU counters                                          |   |
|  |   cache-misses | branch-misses | instructions | cycles      |   |
|  |        |                                                    |   |
|  |        v                                                    |   |
|  |   perf_event_open syscall                                   |   |
|  |                                                             |   |
|  +-------------------------------------------------------------+   |
|                              |                                     |
|                              v                                     |
|   bm_per_kb incresed by clustering %                               |
+========================================+===========================+
                                         |
                                         v
        OUTPUT LINE:
        avg bm_per_kb, Modality, clustering % , web dashboard
```

### Layered view

```text
+--------------------------------------------------------------+
|  Application | Genomic comparator + feature extraction       |
+--------------------------------------------------------------+
|  Workload    | Controlled deterministic C kernel             |
+--------------------------------------------------------------+
|  OS          | Linux, perf_event_open, CPU affinity          |
+--------------------------------------------------------------+
|  Hardware    | ARM CPU core 2, caches, branch predictor, PMU |
+--------------------------------------------------------------+
```

---

## 📁 Project Architecture (Repository)

GENOVA is organised **one folder per gene**. Each gene folder has its own reference sequence, dataset generator, sender script, and a `testdata/` folder of controlled mutation levels. The same kernel and pipeline are reused for every gene.

```text
.
├── genova_pi.c                 # Deterministic kernel + PMU telemetry (runs on the Pi)
│
├── kras/                       # ---- Gene 1: KRAS ----
│   ├── kras_ref.fasta          # KRAS reference sequence (FASTA)
│   ├── kras_reference.bin      # Encoded reference (0..3 per base)
│   ├── makekras.py             # Generates mutated KRAS datasets
│   ├── sendkras.py             # Streams KRAS data to the Pi over TCP
│   ├── send.sh                 # Helper script for sending datasets
│   └── testdata/               # Controlled mutation levels (percent)
│       ├── krasmut0.0005.bin
│       ├── krasmut0.008.bin
│       ├── krasmut0.049.bin
│       ├── krasmut0.12.bin
│       ├── krasmut0.49.bin
│       ├── krasmut0.67.bin
│       ├── krasmut0.9.bin
│       ├── krasmut1.1.bin
│       ├── krasmut4.9.bin
│       ├── krasmut5.1.bin
│       ├── krasmut14.9.bin
│       ├── krasmut15.1.bin
│       ├── krasmut24.99.bin
│       ├── krasmut25.bin
│       └── krasmut25.2.bin
│
├── tp53/                       # ---- Gene 2: TP53 ----
│   ├── tp53_RefSeqGene.fasta   # TP53 RefSeqGene reference (FASTA)
│   ├── tp53_reference.bin      # Encoded reference (0..3 per base)
│   ├── maketp53.py             # Generates mutated TP53 datasets
│   ├── make_classes.py         # Generates class-based datasets
│   ├── sendtp53.py             # Streams TP53 data to the Pi over TCP
│   ├── send.sh                 # Helper script for sending datasets
│   └── testdata/               # Controlled mutation levels (percent)
│       ├── tp53mut0.0005.bin
│       ├── tp53mut0.008.bin
│       ├── tp53mut0.049.bin
│       ├── tp53mut0.12.bin
│       ├── tp53mut0.49.bin
│       ├── tp53mut0.67.bin
│       ├── tp53mut0.9.bin
│       ├── tp53mut1.1.bin
│       ├── tp53mut4.9.bin
│       ├── tp53mut5.1.bin
│       ├── tp53mut14.9.bin
│       ├── tp53mut15.1.bin
│       ├── tp53mut24.99.bin
│       └── tp53mut25.2.bin
│
└── README.md
```

### Folder roles

| Item | Role |
|------|------|
| `*_ref.fasta` / `*_RefSeqGene.fasta` | Source reference sequence for the gene |
| `*_reference.bin` | Reference encoded as one byte per base (A,C,G,T → 0..3) |
| `make*.py` | Builds the mutated sample files with controlled mutation rates |
| `send*.py` | Sends reference + sample chunks to the Pi (TCP, port 9999) |
| `send.sh` | Helper script for sending datasets |
| `testdata/` | One `.bin` file per mutation level, named `<gene>mut<percent>.bin` |

### Mutation levels in `testdata/`

The sweep covers a wide range, from very rare to very frequent changes:

| Band | Mutation rates (%) |
|------|--------------------|
| **Ultra-low** | 0.0005, 0.008, 0.049 |
| **Low** | 0.12, 0.49, 0.67, 0.9 |
| **Moderate** | 1.1, 4.9, 5.1 |
| **High** | 14.9, 15.1 |
| **Very high** | 24.99, 25, 25.2 (KRAS only for 25) |

The pairs (4.9 / 5.1), (14.9 / 15.1) and (24.99 / 25 / 25.2) sit just below and just above round thresholds, which makes them useful for checking whether the hardware signature changes sharply near a boundary.

---

## ⚙️ The Deterministic Kernel (`genova_pi.c`)

> **GENOVA's kernel is a *controlled, deterministic* genomic-processing kernel written in C.**
> Given the same input, it executes the same instruction path every time. Any change in hardware behaviour therefore comes from the **input data**, not from non-deterministic software.

### Design properties

| Property | Implementation |
|----------|----------------|
| **Determinism** | Fixed LCG constants, fixed seeds, no `rand()` in the kernel, no dynamic branching on external state |
| **Isolation** | Pinned to **CPU core 2** via `sched_setaffinity` |
| **Clean measurement** | `exclude_kernel = 1`, `exclude_hv = 1` → user-space only counting |
| **Alignment** | `posix_memalign` with 64-byte (cache-line) alignment |
| **Whole-session counting** | Counters reset + enabled once, disabled after the final chunk |
| **Ground truth** | Software mismatch counter independently verifies the true mismatch count |

### Core algorithm — pairwise reference-vs-sample comparator

```
for each base i:
    diff = ref[i] XOR sample[i]
    if diff == 0:   → MATCH     : cheap, predictable path (acc += 1)
    else:           → MISMATCH  : call chaos(), a data-dependent
                                  branch + memory stress routine
```

- **Match** → predictable branch, minimal cost.
- **Mismatch** → diverges to a heavier path. The *positions and density* of mismatches shape cache and branch behaviour.

### The `chaos()` amplifier (per mismatch)

| Parameter | Value | Purpose |
|-----------|-------|---------|
| `CHAOS_BRANCHES` | **64** | Data-dependent branches → ~64 potential branch mispredicts per mismatch |
| `CHAOS_MEMS` | **8** | Pseudo-random reads → ~8 potential cache misses per mismatch |
| `BOMB_BITS` | **24** | 16 MiB buffer — larger than L3, smaller than RAM |
| `BOMB_SIZE` | `1 << 24` | Forces genuine cache-miss traffic |
| LCG multiplier | `6364136223846793005` | 64-bit deterministic pseudo-random sequence |
| Hash multiplier | `2654435761` | Knuth multiplicative hash for memory index |

### Kernel configuration constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `PORT` | `9999` | TCP listening port |
| `BUF_SIZE` | `1 << 20` (1 MiB) | Max chunk size accepted |
| `LUT_BITS` / `LUT_SIZE` | `18` / `262144` | Look-up table (allocated; no longer used by the comparator) |
| CPU pin | `core 2` | Isolated measurement core |

---

## 📡 The DNA Sender (`send_dna.py`)

```bash
python3 send_dna.py <PI_IP> <healthy|mutX|allA|file.bin> [N]
```

| Mode | Description |
|------|-------------|
| `healthy` | Reference == sample (0% mismatch) → **baseline** |
| `mutX` | Random point substitutions at **X %** (e.g. `mut0.01` = 0.01 %). Fixed seeds → reproducible |
| `allA` | Sample is all `A` (0) vs. random reference → worst-case divergence |
| `*.bin` | Sample loaded from file (e.g. KRAS datasets) and compared against `kras_reference.bin` |

| Parameter | Default | Notes |
|-----------|---------|-------|
| `N` | `5,000,000` | Number of bases (synthetic modes) |
| `CHUNK` | `65536` | Bases per chunk |
| Base encoding | `0..3` | A / C / G / T as one byte each |
| Seeds | `42` (sequence), `12345` (mutation positions) | Reproducibility |
| Connection | Retry for up to 60 s | Sender waits for the Pi to start listening |

---

## 🔬 PMU Signature & Hardware Feature Vector

### Raw counters (measured today)

| Counter | Linux event | What it tells us |
|---------|-------------|------------------|
| **Cache misses** (`cm`) | `PERF_COUNT_HW_CACHE_MISSES` | Memory-system pressure |
| **Branch mispredictions** (`bm`) | `PERF_COUNT_HW_BRANCH_MISSES` | Control-flow irregularity |
| **Instructions** (`ic`) | `PERF_COUNT_HW_INSTRUCTIONS` | Work retired |
| **CPU cycles** (`cy`) | `PERF_COUNT_HW_CPU_CYCLES` | Time cost |

### Derived parameters (computed today)

| Feature | Formula | Meaning |
|---------|---------|---------|
| **IPC** | `instructions / cycles` | Pipeline efficiency |
| **cm_per_kb** | `1000 × cache_misses / bases` | Cache misses per 1000 bases |
| **bm_per_kb** | `1000 × branch_misses / bases` | Branch mispredicts per 1000 bases |
| **mismatch %** | `100 × mismatches / bases` | Software ground truth |
| **bases** | total bases processed | Normalization denominator |

### Extended parameters (proposed for the feature vector)

| Feature | Formula | Why it helps |
|---------|---------|--------------|
| **CPI** | `cycles / instructions` | Inverse of IPC; cost per instruction |
| **Instructions per base** | `instructions / bases` | Work-per-base scaling |
| **Cycles per base** | `cycles / bases` | Time-per-base scaling |
| **Cache-miss rate** | `cache_misses / instructions` | Memory-boundedness |
| **Branch-miss rate** | `branch_misses / instructions` | Control-flow boundedness |
| **Δ vs. baseline (absolute)** | `feature − baseline` | Direct deviation |
| **Δ vs. baseline (relative %)** | `100 × (feature − baseline) / baseline` | Scale-free deviation |
| **Misses per mismatch** | `Δcache_misses / mismatches` | Marginal hardware cost of one variant |
| **Branch-miss per mismatch** | `Δbranch_misses / mismatches` | Marginal control-flow cost of one variant |
| **Z-score vs. baseline** | `(x − μ) / σ` (over repeated runs) | Statistical significance |
| *(optional)* L1D / LLC / dTLB misses, stalled cycles | extra PMU events | Richer signature |
| *(optional)* wall time, CPU temperature | system sensors | Environmental control |

### Signature as a vector

```
V = [ IPC, cm_per_kb, bm_per_kb, CPI, ins/base, cyc/base, cm/ins, bm/ins ]

Signature = V_sample − V_baseline
```

### Output format

```
bases=5000000 mismatch=500 (0.010%) cm=... bm=... ic=... cy=... ipc=... cm_per_kb=... bm_per_kb=...
```

---

## 🔌 Wire Protocol

```
┌──────────────┬───────────────────┬───────────────────┐
│ 4 bytes (LE)│  n bytes          │  n bytes          │
│ chunk length│  reference chunk  │  sample chunk     │
└──────────────┴───────────────────┴───────────────────┘
        ⟲ repeated per chunk (≤ 1 MiB each)

        0x00000000  →  end-of-stream sentinel
```

---

## 🚀 Quick Start

### 1. On the Raspberry Pi — build & run

```bash
gcc -O2 -o genova_pi genova_pi.c

# perf_event_open may need relaxed permissions or root
sudo sysctl -w kernel.perf_event_paranoid=1

./genova_pi
# listening on 9999 [pairwise ref-vs-sample], LUT=262144 entries, pinned to core 2
```

### 2. On the host — send data

```bash
# Baseline: identical sequences
python3 send_dna.py <PI_IP> healthy

# Controlled mutation: 0.01% substitutions
python3 send_dna.py <PI_IP> mut0.01

# Worst case
python3 send_dna.py <PI_IP> allA

# Gene datasets (see the kras/ and tp53/ folders for the exact usage of each sender)
cd kras && python3 sendkras.py <PI_IP> testdata/krasmut0.49.bin
cd tp53 && python3 sendtp53.py <PI_IP> testdata/tp53mut0.49.bin
```

### 3. Read the signature

The Pi prints one summary line when the stream ends. Restart `genova_pi` before each run (it serves one session).

---

## 🧪 Experiment: Controlled Mutation Sweep (KRAS and TP53)

The same pipeline is run on two genes, **KRAS** and **TP53**, each with 14 to 15 controlled mutation levels from 0.0005 % up to about 25 %.

| Gene | Reference | Test files | Mutation range |
|------|-----------|------------|----------------|
| **KRAS** | `kras/kras_reference.bin` | `kras/testdata/krasmut*.bin` (15 files) | 0.0005 % → 25.2 % |
| **TP53** | `tp53/tp53_reference.bin` | `tp53/testdata/tp53mut*.bin` (14 files) | 0.0005 % → 25.2 % |

**Procedure**

```text
1. Pick a gene (kras or tp53)
        |
        v
2. Send the reference against itself  ->  baseline signature (0 % mutation)
        |
        v
3. For each file in testdata/ (low -> high mutation rate):
   send reference + mutated sample through GENOVA
        |
        v
4. Record the feature vector for each run
   (IPC, cm_per_kb, bm_per_kb, mismatch %)
        |
        v
5. Plot features vs. mutation rate and compare with the baseline
        |
        v
6. Does the CPU signature track the input change?
   Is the trend consistent across both genes?
```

Repeat each run several times to estimate run-to-run variance before drawing conclusions.

---

## ✅ What GENOVA Is — and Is Not

| GENOVA **is** | GENOVA is **not** |
|---------------|-------------------|
| A research prototype exploring hardware-level side-signals of genomic data | A diagnostic tool |
| A potential fast **screening / triage layer** for research environments | A replacement for biopsy, sequencing, or clinical diagnosis |
| A study of input-dependent microarchitectural behaviour | A claim that a cache-miss value "means cancer" |
| An edge-computing friendly approach (Raspberry Pi) | Validated on real tumour data (yet) |

> We are not trying to replace existing diagnosis.
> **We are asking a different question: what if the CPU processing the DNA can itself provide an additional signal about what is happening in that genomic workload?**

---

## 🗺️ Roadmap

- [x] Deterministic C kernel with PMU telemetry on Raspberry Pi
- [x] TCP streaming pipeline (host → edge)
- [x] Controlled mutation sweep on **KRAS** and **TP53** (0.0005 % → ~25 %)
- [ ] Repeated runs with statistics (mean, σ, confidence intervals)
- [ ] Expanded PMU feature set (L1D, LLC, dTLB, stalled cycles)
- [ ] Real **tumour vs. normal** genomic datasets through the same pipeline
- [ ] Classifier on hardware feature vectors (threshold → ML)
- [ ] Cross-hardware validation (different Pi models / ARM CPUs)
- [ ] Integration as a triage layer alongside existing genomic workflows

---

## ⚠️ Limitations & Honest Notes

- **Synthetic amplification.** `chaos()` deliberately amplifies mismatch-driven branch and cache behaviour so differences are measurable. The signal comes from a *controlled, engineered workload*; its biological relevance on real data remains to be shown.
- **Reference-aware.** The kernel compares a sample against a reference. The signature reflects *divergence from the reference*, which can be computed in software directly; the open question is whether the hardware signature adds information or robustness beyond that.
- **Naming.** `cm_per_kb` / `bm_per_kb` are computed per **1000 bases**.
- **Noise.** PMU counts vary with temperature, frequency scaling, and background load. Repeated runs, fixed governors, and an isolated core are recommended.
- **Single-session server.** `genova_pi` handles one connection and exits.
- **`send_dna.py` `.bin` mode** pairs the loaded file (sample) with `kras_reference.bin` (reference), and the two must be the same length.
- **Research use only.** Not for clinical decision-making.

---

<div align="center">

**Because in cancer research, every improvement in how quickly we can identify a potentially abnormal signal can create an opportunity to investigate it sooner.**

🧬 *GENOVA — Genomics × Computer Architecture × Edge Computing* 🍓

</div>
