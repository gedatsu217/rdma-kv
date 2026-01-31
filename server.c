#include <stdio.h>
#include "kvlib.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return -1;
    }
    kv_init_server(argv[1]);
    return 0;
}