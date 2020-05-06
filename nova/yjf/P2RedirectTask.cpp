#include "P2RedirectTask.h"
#include "BasicConnection.h"


P2RedirectTask::P2RedirectTask(BasicConnection* clientConnection, char* buffer, int from_app_len, int client_socket)
{
	this->clientConnection = clientConnection;
	int bufferLen = 1024;
	this->buffer = new char[bufferLen];
	for (int i = 0; i < bufferLen; i++) {
		this->buffer[i] = buffer[i];
	}
	this->from_app_len = from_app_len;
	this->client_socket = client_socket;
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
	
	string redirectCommand = commands[4];
	delimiter = "#";

	pos = 0;
	commands.clear();
	while ((pos = redirectCommand.find(delimiter)) != std::string::npos) {
		token = redirectCommand.substr(0, pos);
		commands.push_back(token);
		redirectCommand.erase(0, pos + delimiter.length());
	
	}
	commands.push_back(redirectCommand);
	cout << "command type:" << commands[0] << endl;
	cout << "redis name:" << commands[1] << endl;
	cout << "memery address:" << commands[2] << endl;
	cout << "memery offset:" << commands[3] << endl;
	cout << "data length:" << commands[4] << endl;

	// using above information to get data from rdma:

	return 0;
}


P2RedirectTask::~P2RedirectTask()
{
}
