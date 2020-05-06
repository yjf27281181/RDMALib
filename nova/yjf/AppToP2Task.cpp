#include "AppToP2Task.h"



AppToP2Task::AppToP2Task(BasicConnection* clientConnection, int client_socket, RDMAManager *rdmaManager)
{
	this->clientConnection = clientConnection;
	this->from_app_len = from_app_len;
	this->client_socket = client_socket;
	this->rdmaManager = rdmaManager;
}

int AppToP2Task::Run()
{
	//BasicConnection * redisConnection = new BasicConnection("192.168.0.200", 7000);
	//redisConnection->connectToServer();
	printf("in apptop2task run\n");
	while (true) {
		int from_app_len = read(client_socket, buffer, 1024);
		if (from_app_len <= 0) {
			continue;
		}
		
		char from_redis[1024] = { 0 };
		/*send back a redirect msg*/
		

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
		string cmd = commands[2];
		cout << "show command:" << cmd << endl;
		cout << "app buffer content:" << buffer << endl;
		//int len_from_redis = redisConnection->sendMsgToServer(buffer, from_app_len, from_redis, -1);
		bool isNetworkBusy = true;
		//cout << "from_redis: " << from_redis << endl;
		//if network is busy and the command is get, redirect
		char content[] = "testContent";
		if (cmd == "GET" && isNetworkBusy) {
			string instruction = rdmaManager->writeContentToRDMA(content);
			RDMA_LOG(INFO) << fmt::format("apptop2task(): instruction {}", instruction);
			//save the content in the rdma, return address, offset, length to the p2 client.
			char* redirectCmd = constructRedisReturn("redirect#redis01#1023423#0#1024"); //return sample
			int cmd_len = strlen(redirectCmd);
			clientConnection->sendMsgToServer(redirectCmd, cmd_len, buffer, client_socket);
		}
		else {
			//clientConnection->sendMsgToServer(from_redis, len_from_redis, buffer, client_socket);
		}
	}
	return 0;
}


AppToP2Task::~AppToP2Task()
{
}

char * AppToP2Task::constructRedisReturn(string str)
{
	string res = "";
	res.push_back((char)36);
	res.append(to_string(str.length()));
	res.push_back((char)13); //carriage return 
	res.push_back((char)10); //new line
	res.append(str); 
	res.push_back((char)13); //carriage return 
	res.push_back((char)10); //carriage return 
	printf("--------------!!!%s %d", (char*)res.data());
	return (char*)res.data();
}
