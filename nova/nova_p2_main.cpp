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

    // used to record received message
    deque<string> recv_history_;

    bool
    ProcessRDMAWC(ibv_wc_opcode type, uint64_t wr_id, int remote_server_id,
                  char *buf, uint32_t imm_data) override {
        string bufContent(buf);
        RDMA_LOG(INFO) << fmt::format("t:{} wr:{} remote:{} buf:\"{}\" imm:{}",
                                      ibv_wc_opcode_str(type), wr_id,
                                      remote_server_id, bufContent, imm_data);
        // if (type == IBV_WC_RECV) { // TODO needs fixing
        if (true) {
            this->recv_history_.push_back(bufContent);
        }
        return true;
    }

};

class ExampleRDMAThread {
private:
    NovaMemManager *nmm_;
    NovaRDMARCBroker *broker_;
    P2MsgCallback *p2mc_;
    // const uint32_t my_server_id_;

public:
    ExampleRDMAThread(NovaMemManager*);
    void Start();

    void ReceiveRDMAReadInstruction();
    void ExecuteRDMARead(string instruction);

    RdmaCtrl *ctrl_;
    std::vector<QPEndPoint> endpoints_;
    char *rdma_backing_mem_;
    char *circular_buffer_;
};

ExampleRDMAThread::ExampleRDMAThread(NovaMemManager *mem_manager) {
    nmm_ = mem_manager;
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
void ExampleRDMAThread::Start() {
// A thread i at server j connects to thread i of all other servers.
    p2mc_ = new P2MsgCallback;
    broker_ = new NovaRDMARCBroker(circular_buffer_, 0,
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
                                    p2mc_);
    broker_->Init(ctrl_);

    if (FLAGS_server_id == 0) {
        // step 1- setup a memory block to store data item
        uint32_t scid = nmm_->slabclassid(0, 40);
        char *databuf = nmm_->ItemAlloc(0, scid); // allocate an item of "size =
                                                  // 40 bytes" slab class
        // Do sth with the buf.
        databuf = "this is my data\0";

        int server_id = 1;
        char *sendbuf = broker_->GetSendBuf(server_id);// DONE: check what's the
                                                       // size of this message
                                                       // buffer. Data buffer is
                                                       // clearly 40-bytes.
                                                       // Seems it's set in cmd-
                                                       // line args, and it's
                                                       // 1024 bytes. Should be
                                                       // sufficient to hold the
                                                       // message.

        // Write a request into the buf.
        // Goal: Request i.e. sendbuf = "P2GET 0 0x480f56 15"
        // ("COMMAND THIS_SERVER_ID MEM_ADDR LENGTH_TO_READ")
        // Mind overflow! *sendbuf should hold 1024 bytes.
        // Part 1- COMMAND
        string thisPart = "P2GET";
        // use for loop to deep-copy
        size_t j = 0; // add'l counter to concatenate 4 parts of entire msg
        for (size_t i; i < thisPart.length(); i++) {
            sendbuf[j] = thisPart[i];
            j++;
        }
        sendbuf[j] = ' ';
        j++; // point to the next empty space in sendbuf
        // Part 2- THIS_SERVER_ID
        stringstream ss1;
        ss1 << FLAGS_server_id;
        ss1 >> thisPart;
        for (size_t i; i < thisPart.length(); i++) {
            sendbuf[j] = thisPart[i];
            j++;
        }
        sendbuf[j] = ' ';
        j++;
        // Part 3- MEM_ADDR
        stringstream ss2;
        // ss2 << (void*)databuf; // TODO!!!!
        ss2 << (uint64_t)databuf;
        ss2 >> thisPart;
        for (size_t i; i < thisPart.length(); i++) {
            sendbuf[j] = thisPart[i];
            j++;
        }
        sendbuf[j] = ' ';
        j++;
        // Part 4- LENGTH_TO_READ
        size_t datalen = strlen(databuf);
        stringstream ss3;
        ss3 << datalen;
        ss3 >> thisPart;
        for (size_t i; i < thisPart.length(); i++) {
            sendbuf[j] = thisPart[i];
            j++;
        }
        // Finished building message string (sendbuf).

        uint64_t wr_id = broker_->PostSend(sendbuf, strlen(sendbuf), server_id, 1);
        RDMA_LOG(INFO) << fmt::format("PostSend(): sendbuf \"{}\", wr:{} imm:1", sendbuf, wr_id);
        broker_->FlushPendingSends(server_id);
        broker_->PollSQ(server_id);
        broker_->PollRQ(server_id);
    }

    else { // should be node-1 executing this block
        ReceiveRDMAReadInstruction();
        // if (p2mc_->recv_history_.size() != 0) {
        //     RDMA_LOG(INFO) << fmt::format("i'm node-1, entry point 1, recv_history_ first element is \"{}\"", p2mc_->recv_history_[0]);
        //     p2mc_->recv_history_.pop_front();
        //     // initiate_read();
        // }

    }

    while (true) {
        broker_->PollRQ();
        broker_->PollSQ();
        if (FLAGS_server_id == 1) {
            // string recv_msg = "";
            // if (p2mc_->recv_history_.size() != 0) {
            //     // The line below gets executed. Therefore, when node-1 actually
            //     // receives message "0x48f...", it's already running in here.
            //     RDMA_LOG(INFO) << fmt::format("i'm node-1, entry point 2, recv_history_ first element is \"{}\"", p2mc_->recv_history_[0]);
            //     recv_msg = p2mc_->recv_history_[0];
            //     p2mc_->recv_history_.pop_front();
            // }
            // else {
            //     continue; // do nothing when no message is received!
            // }
            ReceiveRDMAReadInstruction();
        }
        else {
            continue; // do nothing if i'm node-0 (server 0)
        }
    }
}

void ExampleRDMAThread::ReceiveRDMAReadInstruction() {
    assert(FLAGS_server_id == 1); // let upper level ensure this
    if (p2mc_->recv_history_.size() != 0) {
        string recvMsg = p2mc_->recv_history_[0];
        RDMA_LOG(INFO) << fmt::format("i'm node-1, entry point 2, popping recv_history_[0]: \"{}\"", recvMsg);
        p2mc_->recv_history_.pop_front();
        // Now initiate the RDMA read operation, using recvMsg as instruction
        ExecuteRDMARead(recvMsg);
    }
}

void ExampleRDMAThread::ExecuteRDMARead(string instruction) {
    // TODO how do I do sanity check?

    // I really need this to work FAST, hence hard-coded indexing
    // NOTE: assume 1-digit server id
    // instruction look like: "P2GET 0 0x480f56 15"
    //                         [0    [6[8       [18
    // ("COMMAND SUPPLIER_SERVER_ID MEM_ADDR LENGTH_TO_READ")
    assert(instruction.substr(0, 5) == "P2GET"); // might remove to run faster

    /*
    // string supplierServerID = instruction.substr(6, 1);
    int supplierServerID = stoi(instruction.substr(6, 1));
    string memAddr = instruction.substr(8, 8);
    // char memAddrCStr[] = memAddr.c_str(); // compiler complains since unknown size
    char memAddrCStr[8];
    strcpy(memAddrCStr, &memAddr[0]);
    unsigned long int memAddrUL = strtoul(memAddrCStr, NULL, 16);
    // string length = instruction.substr(17);  // this reads [17, end)
    uint32_t length = stoi(instruction.substr(17));
    */

    stringstream ss;
    ss << instruction;
    int supplierServerID;
    ss >> supplierServerID;
    uint64_t memAddr;
    ss >> memAddr;
    uint32_t length;
    ss >> length;
    RDMA_LOG(INFO) << fmt::format("ExecuteRDMARead(): supplier_server_id: {}, mem_addr: {}, length: {}", supplierServerID, memAddr, length);




    /*
    // RDMA_LOG(INFO) << fmt::format("ExecuteRDMARead(): supplier_server_id: {}, mem_addr: {}, length: {}", supplierServerID, memAddr, length);
    RDMA_LOG(INFO) << fmt::format("ExecuteRDMARead(): supplier_server_id: {}, stoi(mem_addr): {}, length: {}", supplierServerID, stoi(memAddr), length);

    // TODO actually read

    // RDMA Read might work with a memory region that is NOT registerd with RNIC
    // but I'm not sure. To be safe, start with RNIC-registered block, but later
    // on try good o' malloc() to see if I can reduce RNIC-registered memory
    // usage.

    // Additionally, instead of using broker_->GetSendBuf(), try ItemAlloc()
    // first.


    // uint64_t PostRead(char *localbuf, uint32_t size, int remote_server_id,
    //                   uint64_t local_offset,
    //                   uint64_t remote_addr, bool is_remote_offset);

    uint32_t scid = nmm_->slabclassid(0, 2000); // using 2000 ( >> 40) results
                                                // in a different slab class
                                                // which doesn't collide with
                                                // *readbuf from main()
    char *readbuf = nmm_->ItemAlloc(0, scid);
    RDMA_LOG(INFO) << fmt::format("PostRead(): readbuf before read: \"{}\"", readbuf); // examine if readbuf contains stuff to begin with
    RDMA_LOG(INFO) << fmt::format("Attempting to read from: \"{}\"", (uint64_t)memAddrUL); // examine if readbuf contains stuff to begin with
    // try with local_offset = 0 (should be correct)
    // uint64_t wr_id = broker_->PostRead(readbuf, length, supplierServerID, 0, (reinterpret_cast<char*>(memAddr)), false);

    // TODO!!! for some reason the line below keeps erroring out with:
    // node-1 (read initiator)
    // [nova_rdma_rc_broker.cpp:328] Assertion! rdma-rc[0]: SQ error wc status 10 str:remote access error serverid 0
    // node-0 (read receiver)
    // [nova_rdma_rc_broker.cpp:379] Assertion! rdma-rc[0]: RQ error wc status Work Request Flushed Error

    uint64_t wr_id = broker_->PostRead(readbuf, 3, supplierServerID, 0, (uint64_t)memAddrUL, false);

    // TODO!! there is no elegant way to convert remote server memory addresses
    // (where to read from) to a string, and convert it back. Try using uint64_t
    // from the very beginning, with some format-specified printing in RDMA_LOG
    // should make things work.

    // RDMA_LOG(INFO) << fmt::format("PostRead(): readbuf \"{}\", wr:{} imm:1", readbuf, wr_id);


    // broker_->FlushPendingSends(supplierServerID);
    */
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
    NovaConfig::config->max_msg_size = FLAGS_rdma_max_msg_size;                                                         // ML: set to 1024
    NovaConfig::config->rdma_max_num_sends = FLAGS_rdma_max_num_sends;                                                  // ML: set to 128 in command-line execution

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
    buf = "nodata\0";
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
