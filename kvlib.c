#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <error.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>

#include "kvlib.h"


struct kv_context* kv_init_server(char* ip_addr) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr(ip_addr);
    struct rdma_cm_event *event;
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    int ret;

    struct kv_context* ctx = (struct kv_context*)malloc(sizeof(struct kv_context));
    if (!ctx)
        return NULL;
    memset(ctx, 0, sizeof(struct kv_context));

    ctx->ec = rdma_create_event_channel();
    ret = rdma_create_id(ctx->ec, &ctx->listen_id, NULL, RDMA_PS_TCP);
    if (ret) {
        perror("rdma_create_id failed");
        return NULL;
    }
    ret = rdma_bind_addr(ctx->listen_id, (struct sockaddr*)&server_addr);
    if (ret) {
        perror("rdma_bind_addr failed");
        return NULL;
    }
    ret = rdma_listen(ctx->listen_id, 1);
    if (ret) {
        perror("rdma_listen failed");
        return NULL;
    }
    ret = rdma_get_cm_event(ctx->ec, &event);
    if (ret) {
        perror("rdma_get_cm_event failed");
        return NULL;
    }
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        perror("Unexpected event");
        return NULL;
    }
    ctx->conn_id = event->id;
    rdma_ack_cm_event(event);

    ctx->pd = ibv_alloc_pd(ctx->conn_id->verbs);
    ctx->cq = ibv_create_cq(ctx->conn_id->verbs, 10, NULL, NULL, 0);
    ctx->buffer = (char*)malloc(SLOT_SIZE * NUM_SLOTS);
    memset(ctx->buffer, 0, SLOT_SIZE * NUM_SLOTS);
    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buffer, SLOT_SIZE * NUM_SLOTS, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
    if (!ctx->mr) {
        perror("ibv_reg_mr failed");
        return NULL;
    }

    struct ibv_qp_init_attr qp_attr = {
        .cap = { .max_send_wr = 10, .max_recv_wr = 10, .max_send_sge = 1, .max_recv_sge = 1 },
        .qp_type = IBV_QPT_RC,
        .send_cq = ctx->cq,
        .recv_cq = ctx->cq
    };
    ret = rdma_create_qp(ctx->conn_id, ctx->pd, &qp_attr);
    if (ret) {
        perror("rdma_create_qp failed");
        return NULL;
    }

    conn_param.responder_resources = 1;
    ret = rdma_accept(ctx->conn_id, &conn_param);
    if (ret) {
        perror("rdma_accept failed");
        return NULL;
    }

    ret = rdma_get_cm_event(ctx->ec, &event);
    if (ret) {
        perror("rdma_get_cm_event failed");
        return NULL;
    }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        perror("Unexpected event");
        return NULL;
    }
    rdma_ack_cm_event(event);

    if (send_metadata(ctx) != 0) {
        perror("send_metadata failed");
        return NULL;
    }

    return ctx;
}

struct kv_context* kv_init_client(char* ip_addr) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr(ip_addr);
    struct rdma_cm_event *event;
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    int ret;

    struct kv_context* ctx = (struct kv_context*)malloc(sizeof(struct kv_context));
    if (!ctx)
        return NULL;
    memset(ctx, 0, sizeof(struct kv_context));

    ctx->ec = rdma_create_event_channel();
    ret = rdma_create_id(ctx->ec, &ctx->conn_id, NULL, RDMA_PS_TCP);
    if (ret) {
        perror("rdma_create_id");
        return NULL;
    }
    ret = rdma_resolve_addr(ctx->conn_id, NULL, (struct sockaddr*)&server_addr, 1000);
    if (ret) {
        perror("rdma_resolve_addr");
        return NULL;
    }
    ret = rdma_get_cm_event(ctx->ec, &event);
    if (ret) {
        perror("rdma_get_cm_event");
        return NULL;
    }
    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED)
        return NULL;
    rdma_ack_cm_event(event);

    ret = rdma_resolve_route(ctx->conn_id, 1000);
    if (ret) {
        perror("rdma_resolve_route");
        return NULL;
    }
    ret = rdma_get_cm_event(ctx->ec, &event);
    if (ret) {
        perror("rdma_get_cm_event");
        return NULL;
    }
    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED)
        return NULL;

    ret = rdma_ack_cm_event(event);
    if (ret) {
        perror("rdma_ack_cm_event");
        return NULL;
    }
    
    ctx->pd = ibv_alloc_pd(ctx->conn_id->verbs);
    ctx->cq = ibv_create_cq(ctx->conn_id->verbs, 10, NULL, NULL, 0);
    ctx->buffer = (char*)malloc(SLOT_SIZE);
    memset(ctx->buffer, 0, SLOT_SIZE);
    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buffer, SLOT_SIZE, IBV_ACCESS_LOCAL_WRITE);


    struct ibv_qp_init_attr qp_attr = {
        .cap = { .max_send_wr = 10, .max_recv_wr = 10, .max_send_sge = 1, .max_recv_sge = 1 },
        .qp_type = IBV_QPT_RC,
        .send_cq = ctx->cq,
        .recv_cq = ctx->cq
    };
    ret = rdma_create_qp(ctx->conn_id, ctx->pd, &qp_attr);
    if (ret) {
        perror("rdma_create_qp failed");
        return NULL;
    }

    if (post_metadata_recv(ctx) != 0) {
        perror("post_metadata_recv");
        return NULL;
    }

    conn_param.initiator_depth = 1;
    conn_param.retry_count = 5;
    ret = rdma_connect(ctx->conn_id, &conn_param);
    if (ret) {
        perror("rdma_connect");
        return NULL;
    }

    ret = rdma_get_cm_event(ctx->ec, &event);
    if (ret) {
        perror("rdma_get_cm_event");
        return NULL;
    }
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        perror("Unexpected event");
        return NULL;
    }
    rdma_ack_cm_event(event);

    if (poll_completion(ctx->cq) != 0) {
        perror("poll_completion failed");
        return NULL;
    }

    struct mr_info* out_info = (struct mr_info*)ctx->buffer;
    ctx->addr = (uint64_t)out_info->addr;
    ctx->rkey = out_info->rkey;

    return ctx;
}

int kv_put(struct kv_context *ctx, uint64_t key, void *value) {
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memcpy(ctx->buffer, value, SLOT_SIZE);

    memset(&sge, 0, sizeof(sge));
    sge.addr   = (uintptr_t)ctx->buffer;
    sge.length = SLOT_SIZE;
    sge.lkey   = ctx->mr->lkey;

    memset(&wr, 0, sizeof(wr));
    wr.wr_id      = key;
    wr.opcode     = IBV_WR_RDMA_WRITE;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.sg_list    = &sge;
    wr.num_sge    = 1;

    wr.wr.rdma.remote_addr = ctx->addr + (key * SLOT_SIZE);
    wr.wr.rdma.rkey        = ctx->rkey;

    if (ibv_post_send(ctx->conn_id->qp, &wr, &bad_wr)) {
        perror("ibv_post_send failed");
        return -1;
    }

    poll_completion(ctx->cq);

    return 0;
}

int kv_get(struct kv_context *ctx, uint64_t key, void *dest) {
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&sge, 0, sizeof(sge));
    sge.addr   = (uintptr_t)ctx->buffer;
    sge.length = SLOT_SIZE;
    sge.lkey   = ctx->mr->lkey;

    memset(&wr, 0, sizeof(wr));
    wr.wr_id      = key;
    wr.opcode     = IBV_WR_RDMA_READ;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.sg_list    = &sge;
    wr.num_sge    = 1;

    wr.wr.rdma.remote_addr = ctx->addr + (key * SLOT_SIZE);
    wr.wr.rdma.rkey        = ctx->rkey;

    if (ibv_post_send(ctx->conn_id->qp, &wr, &bad_wr)) {
        perror("ibv_post_send failed");
        return -1;
    }

    poll_completion(ctx->cq);

    memcpy(dest, ctx->buffer, SLOT_SIZE);

    return 0;
}

int poll_completion(struct ibv_cq *cq) {
    struct ibv_wc wc;
    while (ibv_poll_cq(cq, 1, &wc) == 0);
    if (wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "CQ poll error: %s\n", ibv_wc_status_str(wc.status));
        return -1;
    }
    return 0;
}

int post_metadata_recv(struct kv_context *ctx) {
    struct ibv_sge sge;
    memset(&sge, 0, sizeof(sge));
    sge.addr   = (uintptr_t)ctx->buffer;
    sge.length = sizeof(struct mr_info);
    sge.lkey   = ctx->mr->lkey;

    struct ibv_recv_wr rr, *bad_rr;
    memset(&rr, 0, sizeof(rr));
    rr.wr_id = 0;
    rr.next = NULL;
    rr.sg_list = &sge;
    rr.num_sge = 1;

    return ibv_post_recv(ctx->conn_id->qp, &rr, &bad_rr);
}

int send_metadata(struct kv_context *ctx) {
    struct mr_info *info = (struct mr_info*)ctx->buffer;
    info->addr = (uint64_t)ctx->mr->addr;
    info->rkey = ctx->mr->rkey;

    struct ibv_sge sge;
    memset(&sge, 0, sizeof(sge));
    sge.addr   = (uintptr_t)info;
    sge.length = sizeof(struct mr_info);
    sge.lkey   = ctx->mr->lkey;

    struct ibv_send_wr wr, *bad_wr;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0;
    wr.next = NULL;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    if (ibv_post_send(ctx->conn_id->qp, &wr, &bad_wr))
        return -1;

    return poll_completion(ctx->cq);
}