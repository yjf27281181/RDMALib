
//
// Created by Haoyu Huang on 4/1/19.
// Copyright (c) 2019 University of Southern California. All rights reserved.
//

#ifndef RLIB_NOVA_COMMON_H
#define RLIB_NOVA_COMMON_H


#pragma once

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <vector>
#include <atomic>

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <event.h>
#include <list>

#include "rdma_ctrl.hpp"
#include <stdexcept>

namespace nova {
    using namespace std;
    using namespace rdmaio;
#define CONN_SLEEP 50000

/* Initial power multiplier for the hash table */
    std::string ibv_wr_opcode_str(ibv_wr_opcode code);

    std::string ibv_wc_opcode_str(ibv_wc_opcode code);

    struct Host {
        uint32_t server_id;
        string ip;
        int port;
    };

    struct QPEndPoint {
        Host host;
        uint32_t server_id;
        uint32_t thread_id;
    };

    vector<Host> convert_hosts(string hosts_str);
}
#endif //RLIB_NOVA_COMMON_H
