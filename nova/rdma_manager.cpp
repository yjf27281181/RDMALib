#include "rdma_manager.h"


RDMAManager::RDMAManager(NovaMemManager *mem_manager, RdmaCtrl *ctrl_, std::vector<QPEndPoint> endpoints_, char *rdma_backing_mem_, char *circular_buffer_) {
	this->nmm_ = mem_manager;
	this->p2mc_ = new P2MsgCallback();
	uint32_t scid = nmm_->slabclassid(0, 1000); // using 200 ( >> 40) results in a different slab class which doesn't collide with *readbuf from main()
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
                                    p2mc_);
    this->ctrl_ = ctrl_;
    this->endpoints_ = endpoints_;
    this->rdma_backing_mem_ = rdma_backing_mem_;
    this->circular_buffer_ = circular_buffer_;
    RDMA_LOG(INFO) << fmt::format("start initial broker");
    broker_->Init(ctrl_);
    RDMA_LOG(INFO) << fmt::format("end initial broker");

}

void RDMAManager::Start() {
	RDMA_LOG(INFO) << fmt::format("rdma while loop start()");
	while(true) {
		if(readRequests.size() > 0) {

			RdmaReadRequest* curRequest = popRequestFromQueue();
			p2mc_->hmap.insert(pair<string,RdmaReadRequest*>(curRequest->instruction,curRequest));
			RDMA_LOG(INFO) << fmt::format("get first request {}", curRequest->instruction);
			readContentFromRDMA(curRequest);
		}
		broker_->PollRQ();
        broker_->PollSQ();
	}
}

void RDMAManager::addRequestToQueue(RdmaReadRequest* request) {
	addPopMutex.lock();
	readRequests.push(request);
	addPopMutex.unlock();
}

RdmaReadRequest* RDMAManager::popRequestFromQueue() {
	addPopMutex.lock();
	RdmaReadRequest* res = readRequests.front();
	readRequests.pop();
	addPopMutex.unlock();
	return res;
}


string RDMAManager::readContentFromRDMA(RdmaReadRequest* request) {
    // TODO how do I do sanity check?
    // TODO faster (index-based) instruction argument parsing?

    assert(request->instruction.substr(0, 5) == "P2GET"); // TODO remove?
    stringstream ss(request->instruction.c_str());
    string command;
    ss >> command; // TODO command gets "P2GET", how to skip this?
    int supplierServerID;
    ss >> supplierServerID;
    uint64_t memAddr;
    ss >> memAddr;
    uint32_t length;
    ss >> length;
    char* sendBuffer = broker_->GetSendBuf(supplierServerID);
    char tmpArray[request->instruction.length()]; 
    for (int i = 0; i < sizeof(request->instruction); i++) { 
        tmpArray[i] = request->instruction[i]; 
    } 
    memcpy(sendBuffer, tmpArray, sizeof(request->instruction));
    RDMA_LOG(INFO) << fmt::format("ExecuteRDMARead(): supplier_server_id: {}, mem_addr: {}, length: {}", supplierServerID, memAddr, length);
    uint64_t wr_id = broker_->PostRead(request->readBuffer, length, supplierServerID, 0, memAddr, false); // trying with "true" for is_remote_offset
	broker_->FlushPendingSends(supplierServerID);
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
    
    return string(readbuf_);
}

string RDMAManager::writeContentToRDMA(char* content) {

	uint32_t scid = nmm_->slabclassid(0, 200);
    char *buf = nmm_->ItemAlloc(0, scid); // allocate an item of "size=40" slab class
    memcpy(buf, content, 200);
    nmm_->FreeItem(0, buf, scid);
    // finally free it
    // nmm_->FreeItem(0, buf, scid);
    string instruction = "P2GET";
    instruction += " ";
    instruction += std::to_string(FLAGS_server_id);
    instruction += " ";
    instruction += boost::lexical_cast<std::string>((uint64_t)buf);
    instruction += " ";
    instruction += std::to_string(strlen(content));
    return instruction;
}