// ML: first step is to implement NovaMsgCallback interface
#include "rdma_ctrl.hpp"
#include "nova_common.h"
#include "nova_config.h"
#include "nova_rdma_rc_broker.h"
#include "nova_mem_manager.h"

#include <gflags/gflags.h>
#include <string.h>
#include <sstream> // ML: for saving memory address (pointed-to) as a string, so it can be send over RDMA

using namespace std;
using namespace rdmaio;
using namespace nova;

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
    RDMAManager(NovaMemManager *mem_manager, RdmaCtrl *ctrl_, std::vector<QPEndPoint> endpoints_, char *rdma_backing_mem_, char *circular_buffer_);
    string writeContentToRDMA(char* content);
    string readContentFromRDMA(string instruction);

private:
    NovaMemManager *nmm_;
    NovaRDMARCBroker *broker_;
    char *readbuf_;
    RdmaCtrl *ctrl_;
    std::vector<QPEndPoint> endpoints_;
    char *rdma_backing_mem_;
    char *circular_buffer_;
    P2MsgCallback* p2mc_;
};


class P2MsgCallback : public NovaMsgCallback {
public:
    P2MsgCallback();

    // used to record received message
    deque<string> recv_history_;
    bool read_complete_;

    // TODO! figure out why an RDMA READ attempt result in receiving an
    // empty string in here...
    bool ProcessRDMAWC(ibv_wc_opcode type, uint64_t wr_id, int remote_server_id,
                  char *sendBuffer, uint32_t imm_data) override {
        string bufContent(readbuf);
        RDMA_LOG(INFO) << fmt::format("t:{} wr:{} remote:{} buf:\"{}\" imm:{}",
                                      ibv_wc_opcode_str(type), wr_id,
                                      remote_server_id, bufContent, imm_data);
		if (type == IBV_WC_RDMA_READ) {
            RDMA_LOG(INFO) << fmt::format(" READ COMPLETED, buf:\"{}\"", bufContent);
            this->read_complete_ = true;
            // TODO! WHY THE FUCK CAN'T I JUST GET THE READ DATA ALREADY??
        }
        return true;
    }
};