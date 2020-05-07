// ML: first step is to implement NovaMsgCallback interface
#pragma once
#include "rdma_ctrl.hpp"
#include "nova_common.h"
#include "nova_config.h"
#include "nova_rdma_rc_broker.h"
#include "nova_mem_manager.h"

#include <gflags/gflags.h>
#include <string.h>
#include <sstream> // ML: for saving memory address (pointed-to) as a string, so it can be send over RDMA
#include <queue>

#include<unordered_map>
#include <boost/lexical_cast.hpp>
#include <mutex>
using namespace std;
using namespace rdmaio;
using namespace nova;

DECLARE_string(servers);
DECLARE_int64(server_id);

DECLARE_uint64(mem_pool_size_gb);

DECLARE_uint64(rdma_port);
DECLARE_uint64(rdma_max_msg_size);
DECLARE_uint64(rdma_max_num_sends);
DECLARE_uint64(rdma_doorbell_batch_size);
DECLARE_uint64(nrdma_workers);

class RdmaReadRequest {
public:
    RdmaReadRequest(string instruction, char* readBuffer) {
        this->instruction = instruction;
        this->readBuffer = readBuffer;
    }
    mutex readMutex;
    condition_variable cv;
    string instruction;
    char* readBuffer;
    
};

class P2MsgCallback : public NovaMsgCallback {
private:
    std::condition_variable cv;
    NovaMemManager * nmm_;
    NovaRDMARCBroker *broker_;
public:
    P2MsgCallback(NovaMemManager * nmm_, NovaRDMARCBroker *broker_) {
        this->nmm_ = nmm_;
        this->broker_ = broker_;
    }
    unordered_map<string,RdmaReadRequest*> hmap;
    unordered_map<string,uint32_t> scid_map;
    unordered_map<string,char*> buffer_map;
    // used to record received message

    // TODO! figure out why an RDMA READ attempt result in receiving an
    // empty string in here...
    bool ProcessRDMAWC(ibv_wc_opcode type, uint64_t wr_id, int remote_server_id,
                  char *sendBuffer, uint32_t imm_data) override {
        string instruction(sendBuffer);
        RDMA_LOG(INFO) << fmt::format("t:{} wr:{} remote:{} buf:\"{}\" imm:{}",
                                      ibv_wc_opcode_str(type), wr_id,
                                      remote_server_id, instruction, imm_data);

        if (type == IBV_WC_RECV) {
            RDMA_LOG(INFO) << fmt::format("trying to free memery:\"{}\"", instruction);
            std::unordered_map<string, uint32_t>::iterator it_scid;
            std::unordered_map<string, char*>::iterator it_buffer;
            it_scid = scid_map.find(instruction);
            it_buffer = buffer_map.find(instruction);
            if( it_scid == scid_map.end() || it_buffer == buffer_map.end()) {
                RDMA_LOG(INFO) << fmt::format("wrong,trying to free memory, key not found [], key is:\"{}\"", instruction);
            } else {
                nmm_->FreeItem(0, it_buffer->second, scid->second);
            }

            
        }

        if (type == IBV_WC_RDMA_READ) {
            RDMA_LOG(INFO) << fmt::format(" READ COMPLETED, instruction:\"{}\"", instruction);

            std::unordered_map<string, RdmaReadRequest*>::iterator it;
            it = hmap.find(instruction);
            if( it == hmap.end() ) {
                RDMA_LOG(INFO) << fmt::format("wrong, instruction not found, instruction is:\"{}\"", instruction);
            } else {
                RdmaReadRequest* request = it->second;
                std::lock_guard<std::mutex> lk(request->readMutex);
                request->cv.notify_all();
                RDMA_LOG(INFO) << fmt::format(" notify thread, instruction:\"{}\"", instruction);
                hmap.erase(instruction);

                //notify server node to free memerystringstream ss(request->instruction.c_str());
                string command;
                ss >> command; // TODO command gets "P2GET", how to skip this?
                int supplierServerID;
                ss >> supplierServerID;
                uint64_t memAddr;
                ss >> memAddr;
                uint32_t length;
                ss >> length;
                char* supplierSendBuffer = broker_->GetSendBuf(supplierServerID);
                memcpy(supplierSendBuffer, sendBuffer, strlen(sendBuffer));
                uint64_t wr_id = broker->PostSend(supplierSendBuffer, strlen(supplierSendBuffer), server_id, 1);
                broker->FlushPendingSends(supplierServerID);
            }
            
        }
        return true;
    }
};

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
// node-0 to free that memory

class RDMAManager {
public:
    RDMAManager(NovaMemManager *mem_manager, RdmaCtrl *ctrl_, std::vector<QPEndPoint> endpoints_, char *rdma_backing_mem_, char *circular_buffer_);
    string writeContentToRDMA(char* content);
    string readContentFromRDMA(RdmaReadRequest *readRequest);
    void addRequestToQueue(RdmaReadRequest *request);
    RdmaReadRequest* popRequestFromQueue();
    void Start();
    NovaMemManager *nmm_;
private:
    
    NovaRDMARCBroker *broker_;
    char *readbuf_;
    RdmaCtrl *ctrl_;
    std::vector<QPEndPoint> endpoints_;
    char *rdma_backing_mem_;
    char *circular_buffer_;
    P2MsgCallback* p2mc_;
    std::queue<RdmaReadRequest*> readRequests;
    mutex addPopMutex;
    
};




