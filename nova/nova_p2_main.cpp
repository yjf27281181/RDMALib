// Created by Mian Lu on 4/16/20.
// Reusing code from TA Haoyu's example_main.cpp
// Copyright (c) 2020 University of Southern California. All rights reserved.
#include "rdma_ctrl.hpp"
#include "nova_common.h"
#include "nova_config.h"
#include "nova_rdma_rc_broker.h"
#include "nova_mem_manager.h"
#include "rdma_manager.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <assert.h>
#include <csignal>
#include <gflags/gflags.h>
#include <boost/lexical_cast.hpp>
#include "yjf/AppToP2Server.h"
#include "yjf/P2RedirectServer.h"
#include "yjf/Thread.h"

#include <sstream> // ML: for saving memory address (pointed-to) as a string, so it can be send over RDMA

using namespace std;
using namespace rdmaio;
using namespace nova;

NovaConfig *NovaConfig::config;

DEFINE_string(servers, "node-0:11211,node-1:11211,node-2:11211",
              "A list of servers");
DEFINE_int64(server_id, -1, "Server id.");

DEFINE_uint64(mem_pool_size_gb, 0, "Memory pool size in GB.");

DEFINE_uint64(rdma_port, 0, "The port used by RDMA to setup QPs.");
DEFINE_uint64(rdma_max_msg_size, 0, "The maximum message size used by RDMA.");
DEFINE_uint64(rdma_max_num_sends, 0,
              "The maximum number of pending RDMA sends. This includes READ/WRITE/SEND. We also post the same number of RECV events for each QP. ");
DEFINE_uint64(rdma_doorbell_batch_size, 0, "The doorbell batch size.");
DEFINE_uint64(nrdma_workers, 0,
              "Number of rdma threads.");



int main(int argc, char *argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_server_id == -1) {
        exit(0);
    }
    std::vector<gflags::CommandLineFlagInfo> flags;
    gflags::GetAllFlags(&flags);
    for (const auto &flag : flags) {
        printf("%s=%s\n", flag.name.c_str(),
               flag.current_value.c_str());
    }


    char *rdma_backing_mem = (char *) malloc(
            FLAGS_mem_pool_size_gb * 1024 * 1024 * 1024);
    memset(rdma_backing_mem, 0, FLAGS_mem_pool_size_gb * 1024 * 1024 * 1024);
    std::vector<Host> hosts = convert_hosts(FLAGS_servers);

    NovaConfig::config = new NovaConfig;
    NovaConfig::config->nrdma_threads = FLAGS_nrdma_workers;
    NovaConfig::config->my_server_id = FLAGS_server_id;
    NovaConfig::config->servers = hosts;
    NovaConfig::config->rdma_port = FLAGS_rdma_port;
    NovaConfig::config->rdma_doorbell_batch_size = FLAGS_rdma_doorbell_batch_size;
    NovaConfig::config->max_msg_size = FLAGS_rdma_max_msg_size;                 // ML: set to 1024
    NovaConfig::config->rdma_max_num_sends = FLAGS_rdma_max_num_sends;          // ML: set to 128 in command-line execution

    RdmaCtrl *ctrl = new RdmaCtrl(FLAGS_server_id, FLAGS_rdma_port);
    std::vector<QPEndPoint> endpoints;
    for (int i = 0; i < hosts.size(); i++) {
        if (hosts[i].server_id == FLAGS_server_id) {
            continue;
        }
        QPEndPoint endpoint = {};
        endpoint.thread_id = 0;
        endpoint.server_id = hosts[i].server_id;
        endpoint.host = hosts[i];
        endpoints.push_back(endpoint);
    }

    // Each QP contains nrdma_buf_unit() memory for the circular buffer.
    // An RDMA broker uses nrdma_buf_unit() * number of servers memory for its circular buffers.
    // Each server contains nrdma_buf_unit() * number of servers * number of rdma threads for the circular buffers.

    // We register all memory to the RNIC.
    // RDMA verbs can only work on the memory registered in RNIC.
    // You may use nova mem manager to manage this memory.
    char *user_memory = rdma_backing_mem + nrdma_buf_total();
    uint32_t partitions = 1;
    uint32_t slab_mb = 1;
    NovaMemManager *mem_manager = new NovaMemManager(user_memory, partitions,
                                                     FLAGS_mem_pool_size_gb,
                                                     slab_mb);

    // ML: look at above, a design decision is needed here. Do I instantiate
    // NovaMemManager here, or should I move it into the thread class? Thread
    // need to be able to allocate multiple buffers as it sees fit. Would it
    // still work if NovaMemManager instantiation is moved into thread class?
    // ANSWER: just pass-by-reference a NovaMemManager instance to the thread
    // class!
    // ML: this is simply a char*, and it's meaningful-ness is interpreted at ExampleRDMAThread -> initializing NovaRDMARCBroker
    RDMAManager *rdmaManager = new RDMAManager(mem_manager, ctrl, endpoints, rdma_backing_mem, rdma_backing_mem); // with pass-by-pointer
	std::thread t(&RDMAManager::Start, rdmaManager);

    //run server
    CThreadPool Pool(5);
	CTask* appToP2Server = new AppToP2Server(rdmaManager);
	Pool.AddTask(appToP2Server);
	CTask* p2RedirectServer = new P2RedirectServer();
	Pool.AddTask(p2RedirectServer);
    getchar();
    return 0;
}
