#include <stdio.h>
#include "kvlib.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return -1;
    }
    struct kv_context* ctx = kv_init_client(argv[1]);
    int key = 3;
    int value = 123456;
    kv_put(ctx, key, &value);
    printf("Put value: %d\n", value);
    char recv_buf[SLOT_SIZE];
    kv_get(ctx, key, recv_buf);
    printf("Got value: %d\n", *(int*)recv_buf);
    return 0;
}