#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "kvlib.h"

#define ITERATIONS 500000
#define WARMUP 1000

uint32_t rng_state = 123456789;

inline uint32_t fast_rand() {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return -1;
    }

    struct kv_context* ctx = kv_init_client(argv[1]);
    
    char send_buf[SLOT_SIZE];
    char recv_buf[SLOT_SIZE];
    memset(send_buf, 'a', SLOT_SIZE);

    for (int i = 0; i < WARMUP; i++) {
        uint32_t send_key = fast_rand() & (NUM_SLOTS - 1);
        uint32_t recv_key = fast_rand() & (NUM_SLOTS - 1);
        kv_put(ctx, send_key, send_buf);
        kv_get(ctx, recv_key, recv_buf);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < ITERATIONS; i++) {
        uint32_t send_key = fast_rand() & (NUM_SLOTS - 1);
        uint32_t recv_key = fast_rand() & (NUM_SLOTS - 1);
        kv_put(ctx, send_key, send_buf);
        kv_get(ctx, recv_key, recv_buf);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    int total_ops = ITERATIONS * 2;
    double throughput = total_ops / elapsed_sec;
    double avg_latency_us = (elapsed_sec / total_ops) * 1e6;

    printf("\n--- Benchmark Result ---\n");
    printf("Total Time    : %.3f seconds\n", elapsed_sec);
    printf("Throughput    : %.3f ops/sec (IOPS)\n", throughput);
    printf("Avg Latency   : %.3f us per op\n", avg_latency_us);
    
    return 0;
}