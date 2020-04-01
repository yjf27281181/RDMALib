
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
#include "city_hash.h"

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <event.h>
#include <list>

#include "rdma_ctrl.hpp"
#include <stdexcept>

namespace nova {
#define RDMA_POLL_MIN_TIMEOUT_US 10
#define RDMA_POLL_MAX_TIMEOUT_US 100
#define LEVELDB_TABLE_PADDING_SIZE_MB 2

    using namespace std;
    using namespace rdmaio;

#define EPOLL_MAX_EVENT 1024
#define EPOLL_WAIT_TIME 0


#define GRH_SIZE 40
#define EWOULDBLOCK_SLEEP 10000
#define DEBUG_KEY_SIZE 36
#define DEBUG_VALUE_SIZE 365
#define CONN_SLEEP 50000

#define NOVA_LIST_BACK_ARRAY_SIZE 32
#define NOVA_MAX_CONN 1000000

#define CUCKOO_SEGMENT_SIZE_MB 1

#define CHUNK_ALIGN_BYTES 8
#define MAX_EVICT_CANDIDATES 8
#define MAX_CUCKOO_BUMPS 5

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
