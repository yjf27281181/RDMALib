#include "P2RedirectTask.h"
#include "BasicConnection.h"


P2RedirectTask::P2RedirectTask(BasicConnection* clientConnection, int client_socket, RDMAManager *rdmaManager)
{
	this->clientConnection = clientConnection;
	this->client_socket = client_socket;
	this->rdmaManager = rdmaManager;
}

int P2RedirectTask::Run()
{
	//get data from rmda
	printf("in p2 redirect task: %s \n", this->buffer);

	while(true) {
		int from_app_len = read(client_socket, buffer, 1024);
		if (from_app_len <= 0) {
			continue;
		}
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
		if(strcmp(instruction.c_str(),string("#exit").c_str()) == 0) {
			string constrcutRes = constructRedisReturn("#exit");
			int res_len = constrcutRes.length();
			char res_char_arry[constrcutRes.length()]; 

			for (int i = 0; i < sizeof(res_char_arry); i++) { 
				res_char_arry[i] = constrcutRes[i]; 
			} 
			clientConnection->sendMsgToServer(res_char_arry, strlen(res_char_arry), buffer, client_socket);
			close(clientConnection->server_fd);  
            break;
		}
		uint32_t scid = rdmaManager->nmm_->slabclassid(0, 1000);
	    char *writeBuffer = rdmaManager->nmm_->ItemAlloc(0, scid); // allocate an item of "size=40" slab class
		RdmaReadRequest* request = new RdmaReadRequest(instruction, writeBuffer);
		rdmaManager->addRequestToQueue(request);
		std::unique_lock<std::mutex> lock(request->readMutex);
		request->cv.wait(lock);
		RDMA_LOG(INFO) << fmt::format("writeBuffer content {}", writeBuffer);
		// string contentFromRDMA(writeBuffer);
		// string redisReturn = constructRedisReturn(contentFromRDMA);
		
		clientConnection->sendMsgToServer(writeBuffer, strlen(writeBuffer), buffer, client_socket);
		rdmaManager->nmm_->FreeItem(0, writeBuffer, scid);
		// using above information to get data from rdma:

	}
	

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

	return res;
}


P2RedirectTask::~P2RedirectTask()
{
}
