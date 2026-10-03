#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>

#define PORT        9999
#define BUF_SIZE    (1 << 20)
#define LUT_BITS    18
#define LUT_SIZE    (1 << LUT_BITS)
#define CHAOS_BRANCHES   64          /* ~64 branch mispredicts per mismatch */
#define CHAOS_MEMS       8           /* ~8 cache misses per mismatch */
#define BOMB_BITS        24          /* 16 MB cache bomb (bigger than L3, smaller than RAM) */
#define BOMB_SIZE        (1u << BOMB_BITS)

static unsigned char *bomb_buf;


static int perf_open(struct perf_event_attr *a, pid_t pid, int cpu, int g, unsigned long f) {
    return syscall(__NR_perf_event_open, a, pid, cpu, g, f);
}

static int setup_counter(unsigned int config) {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = config;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    int fd = perf_open(&pe, 0, -1, -1, 0);
    if (fd == -1) { perror("perf_event_open"); exit(1); }
    return fd;
}

static long long rd(int fd) {
    long long v;
    if (read(fd, &v, sizeof(v)) != sizeof(v)) return -1;
    return v;
}


static unsigned long long __attribute__((noinline))
chaos(unsigned long long seed) {
    unsigned long long x = seed | 1ULL;
    unsigned long long acc = 0;

    for (int k = 0; k < CHAOS_BRANCHES; k++) {
        if (x & 1ULL) acc += 1ULL;
        else          acc ^= 3ULL;
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
    }

    unsigned long long idx = acc ^ seed;
    for (int k = 0; k < CHAOS_MEMS; k++) {
        idx = idx * 2654435761ULL + 12345ULL;
        acc += bomb_buf[idx & (BOMB_SIZE - 1)];
    }
    return acc;
}


/* Reference-vs-sample comparator.
   Each iteration: diff = ref ^ sample.
   Match  (diff==0): predictable branch, hits one LUT region.
   Mismatch (diff!=0): different branch, different LUT region.
   The positions of mismatches drive both cache and branch behavior. */
static unsigned long long process_pairwise(const unsigned char *ref,
                                           const unsigned char *sample,
                                           size_t len,
                                           const unsigned int *table) {
    (void)table;                                   /* LUT no longer used */
    unsigned long long acc = 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < len; i++) {
        unsigned int diff = (unsigned int)(ref[i] ^ sample[i]);
        if (diff == 0) {
            acc += 1ULL;                           /* cheap, predictable */
        } else {
            acc ^= chaos((unsigned long long)diff * 0x9E3779B97F4A7C15ULL ^ acc);
        }
    }
    return acc;
}

int main(void) {
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(2, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) { perror("affinity"); return 1; }

    unsigned char *ref_buf, *sam_buf;
    unsigned int  *table;
    if (posix_memalign((void**)&ref_buf, 64, BUF_SIZE) != 0) { perror("ref"); return 1; }
    if (posix_memalign((void**)&sam_buf, 64, BUF_SIZE) != 0) { perror("sam"); return 1; }
    if (posix_memalign((void**)&table,   64, LUT_SIZE * sizeof(unsigned int)) != 0) { perror("tbl"); return 1; }
    if (posix_memalign((void**)&bomb_buf, 64, BOMB_SIZE) != 0) { perror("bomb"); return 1; }   /* NEW */
    memset(bomb_buf, 0xA5, BOMB_SIZE);                                                          /* NEW */

    for (int j = 0; j < LUT_SIZE; j++) table[j] = (unsigned int)(j * 2654435761u + 12345u);

    int fd_cm = setup_counter(PERF_COUNT_HW_CACHE_MISSES);
    int fd_bm = setup_counter(PERF_COUNT_HW_BRANCH_MISSES);
    int fd_in = setup_counter(PERF_COUNT_HW_INSTRUCTIONS);
    int fd_cy = setup_counter(PERF_COUNT_HW_CPU_CYCLES);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(PORT);
    if (bind(srv, (struct sockaddr*)&a, sizeof(a)) == -1) { perror("bind"); return 1; }
    listen(srv, 1);
    fprintf(stderr, "listening on %d [pairwise ref-vs-sample], LUT=%d entries, pinned to core 2\n",
            PORT, LUT_SIZE);
    int cli = accept(srv, NULL, NULL);

    ioctl(fd_cm, PERF_EVENT_IOC_RESET,  0);
    ioctl(fd_bm, PERF_EVENT_IOC_RESET,  0);
    ioctl(fd_in, PERF_EVENT_IOC_RESET,  0);
    ioctl(fd_cy, PERF_EVENT_IOC_RESET,  0);
    ioctl(fd_cm, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(fd_bm, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(fd_in, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(fd_cy, PERF_EVENT_IOC_ENABLE, 0);

    unsigned long long total_ref = 0, total_mismatch = 0;
    unsigned char hdr[4];

    for (;;) {
        if (recv(cli, hdr, 4, MSG_WAITALL) != 4) break;
        unsigned int n = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16) | (hdr[3] << 24);
        if (n == 0) break;
        if (n > BUF_SIZE) { fprintf(stderr, "oversize chunk\n"); break; }

        size_t got = 0;
        while (got < n) { ssize_t r = recv(cli, ref_buf + got, n - got, 0); if (r <= 0) goto done; got += (size_t)r; }
        got = 0;
        while (got < n) { ssize_t r = recv(cli, sam_buf + got, n - got, 0); if (r <= 0) goto done; got += (size_t)r; }

        volatile unsigned long long r = process_pairwise(ref_buf, sam_buf, n, table);
        (void)r;

        /* Count mismatches in software for ground truth */
        for (size_t i = 0; i < n; i++) if (ref_buf[i] != sam_buf[i]) total_mismatch++;
        total_ref += n;
    }

done:
    ioctl(fd_cm, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(fd_bm, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(fd_in, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(fd_cy, PERF_EVENT_IOC_DISABLE, 0);

    long long cm = rd(fd_cm), bm = rd(fd_bm), ic = rd(fd_in), cy = rd(fd_cy);
    double ipc = cy ? (double)ic / (double)cy : 0.0;
    double cmk = total_ref ? 1000.0 * (double)cm / (double)total_ref : 0.0;
    double bmk = total_ref ? 1000.0 * (double)bm / (double)total_ref : 0.0;
    double mmp = total_ref ? 100.0 * (double)total_mismatch / (double)total_ref : 0.0;

    printf("bm_per_kb=%.3f\n",bmk);

    close(cli); close(srv);
    free(ref_buf); free(sam_buf); free(table); free(bomb_buf);
    return 0;
}
