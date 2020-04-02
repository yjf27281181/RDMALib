
//
// Created by Haoyu Huang on 2/24/19.
// Copyright (c) 2019 University of Southern California. All rights reserved.
//

#ifndef NOVA_CC_CONFIG_H
#define NOVA_CC_CONFIG_H

#include <sstream>
#include <string>
#include <fstream>
#include <list>
#include <fmt/core.h>
#include <thread>
#include <syscall.h>
#include <atomic>

#include "nova/rdma_ctrl.hpp"
#include "nova/nova_common.h"

namespace nova {
    using namespace std;
    using namespace rdmaio;

    class NovaConfig {
    public:
        vector<Host> servers;
        int my_server_id;
        int max_msg_size;

        int nrdma_threads;
        int rdma_port;
        int rdma_max_num_sends;
        int rdma_doorbell_batch_size;

        static NovaConfig *config;
    };
    uint64_t nrdma_buf_unit();
    uint64_t nrdma_buf_total();
}
#endif //NOVA_CC_CONFIG_H
