// Created by Mian Lu on 4/16/20.
// Reusing code from TA Haoyu's example_main.cpp
// Copyright (c) 2020 University of Southern California. All rights reserved.

#include "rdma_ctrl.hpp"
#include "nova_common.h"
#include "nova_config.h"
#include "nova_rdma_rc_broker.h"
#include "nova_mem_manager.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <assert.h>
#include <csignal>
#include <gflags/gflags.h>

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
DEFINE_uint32(nrdma_workers, 0,
              "Number of rdma threads.");

// ML: first step is to implement NovaMsgCallback interface
class P2MsgCallback : public NovaMsgCallback {
public:
    bool
    ProcessRDMAWC(ibv_wc_opcode type, uint64_t wr_id, int remote_server_id,
                  char *buf, uint32_t imm_data) override {
        RDMA_LOG(INFO) << fmt::format("t:{} wr:{} remote:{} buf:{} imm:{}",
                                      ibv_wc_opcode_str(type), wr_id,
                                      remote_server_id, buf[0], imm_data);
        return true;
    }
};

class ExampleRDMAThread {
private:
    NovaMemManager *nmm;
public:
    ExampleRDMAThread(NovaMemManager*);
    void Start();

    RdmaCtrl *ctrl_;
    std::vector<QPEndPoint> endpoints_;
    char *rdma_backing_mem_;
    char *circular_buffer_;
};

// ML: added a meaningful constructor
ExampleRDMAThread::ExampleRDMAThread(NovaMemManager *mem_manager) {
    this->nmm = mem_manager;
}

// ML: This is what i should mainly be editing. Let's dictate that node-0 wakes
// up to write some naive data in a known location (or that location can be
// obtained and sent to node-1 at run time). Then, run the same code on node-1
// which wakes up to immediately [look for that location and then] read from
// node-0's that location.
// After reading <Intro to IB> it becomes clearer: node-0 upon detecting that
// itself being node-0, should: 1. register a memory location 2. send that
// location information as a SEND to node-1 3. wait until node-1 somehow signals
// it back that the memory location is good to be freed. On the other hand,
// node-1 should: 1. wait to RECV a msg from node-0 indication memory location,
// 2. use RDMA READ to obtain data from that location 3. use SEND to signal
// node-0 to free that memory.
/*
void ExampleRDMAThread::Start() {
// A thread i at server j connects to thread i of all other servers.
    // ML: for testing
    assert(this->nmm != NULL);
    NovaRDMARCBroker *broker = new NovaRDMARCBroker(circular_buffer_, 0,
                                                    endpoints_,
                                                    FLAGS_rdma_max_num_sends,
                                                    FLAGS_rdma_max_msg_size,
                                                    FLAGS_rdma_doorbell_batch_size,
                                                    FLAGS_server_id,
                                                    rdma_backing_mem_,
                                                    FLAGS_mem_pool_size_gb *
                                                    1024 *
                                                    1024 * 1024,
                                                    FLAGS_rdma_port,
                                                    new P2MsgCallback);
    broker->Init(ctrl_);

    if (FLAGS_server_id == 0) { // ML: if we're node-0 (then our peer is node-1)
        // Step 1- Register a memory block with NovaMemManager
        uint32_t scid = nmm->slabclassid(0, 40);
        char *bufStorage = nmm->ItemAlloc(0, scid); // allocate an item of "size=40" slab class
        // Step 2- Store something in the buf
        bufStorage = "|- rdma read target data -|\0";
        // Step 3- Figure out how to send bufStorage (the address it's pointing
        // at) as a piece of information over RDMA Send

        char *bufMsg = (char*)malloc(64 * sizeof(char)); // TODO remember to free!
        // Verdict: this line above is causing the error! Must use ItemAlloc()
        // char *bufMsg = nmm->ItemAlloc(0, scid); // reuse scid for a 40-byte-or-less message

        // use ostringstream for storing memory address as a string, then
        // converted to char array pointed at by char*, then send bufMsg over
        // the wire
        ostringstream oss;
        oss << (void*)bufStorage; // oss should hold the ADDRESS of bufStorage
        bufMsg = strdup(oss.str().c_str());
        // cout << "bufMsg contains value: " << bufMsg << endl;
        RDMA_LOG(INFO) << fmt::format("bufMsg contains value: {}", bufMsg);
        RDMA_LOG(INFO) << fmt::format("bufMsg lives at: {}", (void*)bufMsg);
        // result of line above:
        // Mon Apr 20 04:36:16 2020, [nova_p2_main.cpp:106] bufMsg contains value: 0x47f8c8

        // TODO now RDMASend() using bufMsg as the outgoing message buffer

        // TODO after RDMASend(), maybe free bufMsg?

        // TODO ideally have the other node notify upon finish reading

        // TODO finally free bufStorage
        // mem_manager->FreeItem(0, buf, scid);

        // for some reason, hit "seg fault"
        int server_id = 1;
        char *sendbuf = broker->GetSendBuf(server_id);
        // Write a request into the buf.
        // sendbuf[0] = bufMsg[0];
        // sendbuf[1] = bufMsg[1]; // this seems to work fine
        sendbuf = "placeholder";
        // *(sendbuf + 8) = *bufMsg;
        uint64_t wr_id = broker->PostSend(sendbuf, 1, server_id, 1);
        // RDMA_LOG(INFO) << fmt::format("send one byte 'a' wr:{} imm:1", wr_id);
        RDMA_LOG(INFO) << fmt::format("send {} bytes wr:{} imm:1", strlen(sendbuf), wr_id);
        broker->FlushPendingSends(server_id);
        broker->PollSQ(server_id);
        broker->PollRQ(server_id);
    }
    else {
        // TODO if we're node-1
        // int server_id = 0; // the peer we're working with
        // broker->PollSQ(server_id);
        // broker->PollRQ(server_id);
        // ML: commented out since example_main doesn't have this; add later
    }

    while (true) {
        // TODO: don't do anything yet
        // broker->PollRQ();
        // broker->PollSQ();
    }
}
*/

// let's first test a few things out. For instance, a longer string than 'a'.
void ExampleRDMAThread::Start() {
// A thread i at server j connects to thread i of all other servers.
    NovaRDMARCBroker *broker = new NovaRDMARCBroker(circular_buffer_, 0,
                                                    endpoints_,
                                                    FLAGS_rdma_max_num_sends,
                                                    FLAGS_rdma_max_msg_size,
                                                    FLAGS_rdma_doorbell_batch_size,
                                                    FLAGS_server_id,
                                                    rdma_backing_mem_,
                                                    FLAGS_mem_pool_size_gb *
                                                    1024 *
                                                    1024 * 1024,
                                                    FLAGS_rdma_port,
                                                    new DummyNovaMsgCallback);
    broker->Init(ctrl_);

    if (FLAGS_server_id == 0) {
        int server_id = 1;
        char *sendbuf = broker->GetSendBuf(server_id);
        // Write a request into the buf.
        sendbuf[0] = 'x';
        sendbuf[1] = 'z';
        uint64_t wr_id = broker->PostSend(sendbuf, 1, server_id, 1);
        RDMA_LOG(INFO) << fmt::format("sendbuf \"{}\", wr:{} imm:1", sendbuf, wr_id);
        broker->FlushPendingSends(server_id);
        broker->PollSQ(server_id);
        broker->PollRQ(server_id);
    }

    while (true) {
        broker->PollRQ();
        broker->PollSQ();
    }
}

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
    NovaConfig::config->max_msg_size = FLAGS_rdma_max_msg_size;
    NovaConfig::config->rdma_max_num_sends = FLAGS_rdma_max_num_sends;

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
    uint32_t scid = mem_manager->slabclassid(0, 40);
    char *buf = mem_manager->ItemAlloc(0, scid); // allocate an item of "size=40" slab class
    // Do sth with the buf.
    buf = "lmao this should fit\0";
    // have another node read from here
    // TODO

    // ideally have the other node notify upon finish reading
    // TODO

    // finally free it
    mem_manager->FreeItem(0, buf, scid);

    // ML: look at above, a design decision is needed here. Do I instantiate
    // NovaMemManager here, or should I move it into the thread class? Thread
    // need to be able to allocate multiple buffers as it sees fit. Would it
    // still work if NovaMemManager instantiation is moved into thread class?
    // ANSWER: just pass-by-reference a NovaMemManager instance to the thread
    // class!
    ExampleRDMAThread *example = new ExampleRDMAThread(mem_manager); // with pass-by-pointer
    example->circular_buffer_ = rdma_backing_mem; // ML: this is simply a char*, and it's meaningful-ness is interpreted at ExampleRDMAThread -> initializing NovaRDMARCBroker
    example->ctrl_ = ctrl;
    example->endpoints_ = endpoints;
    example->rdma_backing_mem_ = rdma_backing_mem;
    std::thread t(&ExampleRDMAThread::Start, example);
    t.join();
    return 0;
}
