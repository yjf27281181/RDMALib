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
#include <boost/lexical_cast.hpp>

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
    P2MsgCallback();

    // used to record received message
    deque<string> recv_history_;
    bool read_complete_;

    // TODO! figure out why an RDMA READ attempt result in receiving an
    // empty string in here...
    bool
    ProcessRDMAWC(ibv_wc_opcode type, uint64_t wr_id, int remote_server_id,
                  char *buf, uint32_t imm_data) override {
        string bufContent(buf);
        RDMA_LOG(INFO) << fmt::format("t:{} wr:{} remote:{} buf:\"{}\" imm:{}",
                                      ibv_wc_opcode_str(type), wr_id,
                                      remote_server_id, bufContent, imm_data);
        if (type == IBV_WC_RECV) {
            this->recv_history_.push_back(bufContent);
        }
        else if (type == IBV_WC_RDMA_READ) {
            RDMA_LOG(INFO) << fmt::format(" READ COMPLETED, buf:\"{}\"", bufContent);
            this->read_complete_ = true;
            // TODO! WHY THE FUCK CAN'T I JUST GET THE READ DATA ALREADY??
        }
        return true;
    }
};

P2MsgCallback::P2MsgCallback() {
    this->read_complete_ = false; // initialize to false on construction
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
class RDMAManager {
public:
	RDMAManager(NovaMemManager *mem_manager);
	string writeContentToRDMA(char* content);
	string readContentFromRDMA(string instruction);
	RdmaCtrl *ctrl_;
    std::vector<QPEndPoint> endpoints_;
    char *rdma_backing_mem_;
    char *circular_buffer_;
private:
	NovaMemManager *nmm_;
    NovaRDMARCBroker *broker_;
    char *readbuf_;
};

RDMAManager::RDMAManager(NovaMemManager *mem_manager) {
	this->nmm_ = mem_manager;
	uint32_t scid = nmm_->slabclassid(0, 2000); // using 2000 ( >> 40) results in a different slab class which doesn't collide with *readbuf from main()
    this->readbuf_ = nmm_->ItemAlloc(0, scid);
    this->broker_ = new NovaRDMARCBroker(circular_buffer_, 0,
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
                                    NULL);

}

string RDMAManager::readContentFromRDMA(string instruction) {
    // TODO how do I do sanity check?
    // TODO faster (index-based) instruction argument parsing?

    assert(instruction.substr(0, 5) == "P2GET"); // TODO remove?
    stringstream ss(instruction.c_str());
    string command;
    ss >> command; // TODO command gets "P2GET", how to skip this?
    int supplierServerID;
    ss >> supplierServerID;
    uint64_t memAddr;
    ss >> memAddr;
    uint32_t length;
    ss >> length;
    RDMA_LOG(INFO) << fmt::format("ExecuteRDMARead(): supplier_server_id: {}, mem_addr: {}, length: {}", supplierServerID, memAddr, length);
    uint64_t wr_id = broker_->PostRead(readbuf_, length, supplierServerID, 0, memAddr, false); // trying with "true" for is_remote_offset

    // There is no elegant way to convert remote server memory addresses
    // (where to read from) to a string, and convert it back. Try using uint64_t
    // from the very beginning, with some format-specified printing in RDMA_LOG
    // should make things work. Update: Haoyu said simply cast to uint64_t from
    // the very beginning, where node-0 constructs char *sendbuf
    // Update 2: not just simply cast to uint64_t after RECV, but also store
    // node-0's message as uint64_t for memAddr. memcpy().

    // this line below is VERY problematic! Basically when hitting this line,
    // RDMA READ is NOT YET complete! Only when msgCallback is hit, that means
    // this readbuf_ should be populated!
    RDMA_LOG(INFO) << fmt::format("PostRead(): readbuf_ right after read attempt \"{}\", wr:{} imm:1", readbuf_, wr_id);
    return string(readbuf_);
}

string RDMAManager::writeContentToRDMA(char* content) {

	uint32_t scid = nmm_->slabclassid(0, 200);
    char *buf = nmm_->ItemAlloc(0, scid); // allocate an item of "size=40" slab class
    strcpy(buf, content);
    // finally free it
    nmm_->FreeItem(0, buf, scid);
    string instruction = "P2GET";
    instruction += " ";
    instruction += std::to_string(FLAGS_server_id);
    instruction += " ";
    instruction += boost::lexical_cast<std::string>((uint64_t)buf);
    instruction += " ";
    instruction += std::to_string(strlen(content));
    return instruction;
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
    RDMAManager *rdmaManager = new RDMAManager(mem_manager); // with pass-by-pointer
    rdmaManager->circular_buffer_ = rdma_backing_mem; // ML: this is simply a char*, and it's meaningful-ness is interpreted at ExampleRDMAThread -> initializing NovaRDMARCBroker
    rdmaManager->ctrl_ = ctrl;
    rdmaManager->endpoints_ = endpoints;
    rdmaManager->rdma_backing_mem_ = rdma_backing_mem;
    char content[] = "Hello"; 
    string instruction = rdmaManager->writeContentToRDMA(content);
    RDMA_LOG(INFO) << fmt::format("main(): instruction {}", instruction);
    string contentFromRDMA = rdmaManager->readContentFromRDMA(instruction);
    RDMA_LOG(INFO) << fmt::format("main(): content {}", contentFromRDMA);
    return 0;
}
