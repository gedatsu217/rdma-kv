#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <stddef.h>

#define SLOT_SIZE 1024
#define NUM_SLOTS 1024

struct kv_context {
    struct rdma_event_channel *ec;
    struct rdma_cm_id *conn_id;
    struct rdma_cm_id *listen_id;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_mr *mr;
    char *buffer;
    uint64_t addr;
    uint32_t rkey;
};

struct mr_info {
    uint64_t addr;
    uint32_t rkey;
};

struct kv_context* kv_init_server(char* ip_addr);
struct kv_context* kv_init_client(char* ip_addr);
int kv_put(struct kv_context *ctx, uint64_t key, void *value);
int kv_get(struct kv_context *ctx, uint64_t key, void *value);
int poll_completion(struct ibv_cq *cq);
int send_metadata(struct kv_context* ctx);
int post_metadata_recv(struct kv_context* ctx);