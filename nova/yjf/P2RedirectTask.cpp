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
		cout <<"token: " << token << endl;
		commands.push_back(token);
		command.erase(0, pos + delimiter.length());
	}
	
	string instruction = commands[4];
	char writeBuffer[1000];
	RdmaReadRequest* request = new RdmaReadRequest(instruction, buffer);
	std::unique_lock<std::mutex> lock(request->readMutex);
	request->cv.wait(lock);
	RDMA_LOG(INFO) << fmt::format("writeBuffer content {}", writeBuffer);
	clientConnection->sendMsgToServer(writeBuffer, strlen(buffer), buffer, client_socket);
	// using above information to get data from rdma:

	return 0;
}


P2RedirectTask::~P2RedirectTask()
{
}
