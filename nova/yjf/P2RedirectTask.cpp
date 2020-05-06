#include "P2RedirectTask.h"
#include "BasicConnection.h"


P2RedirectTask::P2RedirectTask(BasicConnection* clientConnection, char* buffer, int from_app_len, int client_socket, RDMAManager *rdmaManager)
{
	this->clientConnection = clientConnection;
	int bufferLen = 1024;
	this->buffer = new char[bufferLen];
	for (int i = 0; i < bufferLen; i++) {
		this->buffer[i] = buffer[i];
	}
	this->from_app_len = from_app_len;
	this->client_socket = client_socket;
	this->rdmaManager = rdmaManager;
}

int P2RedirectTask::Run()
{
	//get data from rmda
	printf("in p2 redirect task: %s \n", this->buffer);

	string command(buffer);
	string delimiter = "";
	delimiter.push_back((char)13);
	delimiter.push_back((char)10);

	size_t pos = 0;
	string token;
	vector<std::string> commands;
	while ((pos = command.find(delimiter)) != std::string::npos) {
		token = command.substr(0, pos);
		commands.push_back(token);
		command.erase(0, pos + delimiter.length());
	}
	
	string instruction = commands[4];
	uint32_t scid = rdmaManager->nmm_->slabclassid(0, 1000);
    char *writeBuffer = rdmaManager->nmm_->ItemAlloc(0, scid); // allocate an item of "size=40" slab class
	RdmaReadRequest* request = new RdmaReadRequest(instruction, writeBuffer);
	rdmaManager->addRequestToQueue(request);
	std::unique_lock<std::mutex> lock(request->readMutex);
	request->cv.wait(lock);
	RDMA_LOG(INFO) << fmt::format("writeBuffer content {}", writeBuffer);
	string contentFromRDMA(writeBuffer);
	string redisReturn = constructRedisReturn(contentFromRDMA);
	int res_len = redisReturn.length();
	char res_char_arry[redisReturn.length()]; 

	for (int i = 0; i < sizeof(res_char_arry); i++) { 
		res_char_arry[i] = redisReturn[i]; 
	} 
	clientConnection->sendMsgToServer(res_char_arry, res_len, buffer, client_socket);
	// using above information to get data from rdma:

	return 0;
}

string P2RedirectTask::constructRedisReturn(string str) {
	string res = "";
	res.push_back((char)36);
	res.append(to_string(str.length()));
	res.push_back((char)13); //carriage return 
	res.push_back((char)10); //new line
	res.append(str); 
	res.push_back((char)13); //carriage return 
	res.push_back((char)10); //carriage return 
	RDMA_LOG(INFO) << fmt::format("construct instruction {}", res);
	return res;
}


P2RedirectTask::~P2RedirectTask()
{
}
