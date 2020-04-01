
//
// Created by Haoyu Huang on 1/10/20.
// Copyright (c) 2020 University of Southern California. All rights reserved.
//

#include "nova_config.h"

namespace nova {
    uint64_t nrdma_buf_unit() {
        return (NovaConfig::config->rdma_max_num_sends * 2) *
               NovaConfig::config->max_msg_size;
    }

    uint64_t nrdma_buf_total() {
        uint64_t nrdmatotal = nrdma_buf_unit() *
                              NovaConfig::config->nrdma_threads *
                              NovaConfig::config->servers.size();
        return nrdmatotal;
    }
}