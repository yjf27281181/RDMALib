
//
// Created by Haoyu Huang on 4/4/19.
// Copyright (c) 2019 University of Southern California. All rights reserved.
//

#ifndef RLIB_NOVA_RDMA_RC_BROKER_H
#define RLIB_NOVA_RDMA_RC_BROKER_H

#include <fmt/core.h>

#include "rdma_ctrl.hpp"
#include "nova_rdma_broker.h"
#include "nova_msg_callback.h"
#include "nova_common.h"

namespace nova {

    using namespace rdmaio;

    // Thread local. One thread has one RDMA RC Broker.
    class NovaRDMARCBroker : public NovaRDMABroker {
    public:
        NovaRDMARCBroker(char *buf, int thread_id,
                         const std::vector<QPEndPoint> &end_points,
                         uint32_t max_num_sends,
                         uint32_t max_msg_size,
                         uint32_t doorbell_batch_size,
                         uint32_t my_server_id,
                         char *mr_buf,
                         uint64_t mr_size,
                         uint64_t rdma_port,
                         NovaMsgCallback *callback);

        void Init(RdmaCtrl *rdma_ctrl);

        uint64_t PostRead(char *localbuf, uint32_t size, int remote_server_id,
                          uint64_t local_offset,
                          uint64_t remote_addr, bool is_remote_offset);

        uint64_t
        PostSend(const char *localbuf, uint32_t size, int remote_server_id,
                 uint32_t imm_data);

        uint64_t
        PostWrite(const char *localbuf, uint32_t size, int remote_server_id,
                  uint64_t remote_offset, bool is_remote_offset,
                  uint32_t imm_data);

        void FlushPendingSends();

        void FlushPendingSends(int remote_server_id) override;

        uint32_t PollSQ(int remote_server_id);

        uint32_t PollSQ();

        void PostRecv(int remote_server_id, int recv_buf_index);

        void FlushPendingRecvs();

        uint32_t PollRQ();

        uint32_t PollRQ(int remote_server_id);

        char *GetSendBuf();

        char *GetSendBuf(int remote_server_id);

        uint32_t thread_id() { return thread_id_; }

    private:
        uint32_t to_qp_idx(uint32_t remote_server_id);

        uint64_t
        PostRDMASEND(const char *localbuf, ibv_wr_opcode type, uint32_t size,
                     int qp_idx,
                     uint64_t local_offset,
                     uint64_t remote_addr, bool is_offset,
                     uint32_t imm_data);

        const uint32_t my_server_id_;
        const char *mr_buf_;
        const uint64_t mr_size_;
        const uint64_t rdma_port_;
        const uint32_t max_num_sends_;
        const uint32_t max_msg_size_;
        const uint32_t doorbell_batch_size_;

        std::map<uint32_t, int> server_qp_idx_map;
        std::vector<QPEndPoint> end_points_;
        const int thread_id_;
        const char *rdma_buf_;

        // RDMA variables
        ibv_wc *wcs_;
        RCQP **qp_;
        char **rdma_send_buf_;
        char **rdma_recv_buf_;

        struct ibv_sge **send_sges_;
        ibv_send_wr **send_wrs_;
        int *send_sge_index_;

        // pending sends.
        int *npending_send_;
        int *psend_index_;
        NovaMsgCallback *callback_;
    };
}

#endif //RLIB_NOVA_RDMA_RC_BROKER_H
