#include "AppToP2Task.h"



AppToP2Task::AppToP2Task(BasicConnection* clientConnection, int client_socket, RDMAManager *rdmaManager)
{
	this->clientConnection = clientConnection;
	this->client_socket = client_socket;
	this->rdmaManager = rdmaManager;
}

int AppToP2Task::Run()
{
	BasicConnection * redisConnection = new BasicConnection("127.0.0.1", 6379);
	redisConnection->connectToServer();
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
		string key = "";
		string cmd = commands[2];
		if(commands.size() > 4) {
			key = commands[4];
		}
		if(strcmp(key.c_str(),string("#exit").c_str()) == 0) {
			string constrcutRes = constructRedisReturn("#exit");
			int res_len = constrcutRes.length();
			char res_char_arry[constrcutRes.length()]; 

			for (int i = 0; i < sizeof(res_char_arry); i++) { 
				res_char_arry[i] = constrcutRes[i]; 
			} 
			clientConnection->sendMsgToServer(res_char_arry, sizeof(res_char_arry), buffer, client_socket);
			close(clientConnection->server_fd);  
            break;
		}
		int len_from_redis = redisConnection->sendMsgToServer(buffer, from_app_len, from_redis, -1);
		RDMA_LOG(INFO) << fmt::format("redis buffer {}", from_redis);
		bool isNetworkBusy = false;
		//cout << "from_redis: " << from_redis << endl;
		//if network is busy and the command is get, redirect
		//char content[] = "testContent";
		if (cmd == "GET" && isNetworkBusy) {
			string instruction = rdmaManager->writeContentToRDMA(from_redis);
			
			//save the content in the rdma, return address, offset, length to the p2 client.
			string redirectCmd = constructRedisReturn(instruction); //return sample
			int cmd_len = redirectCmd.length();
			char cmd_char_arry[redirectCmd.length()]; 
  
    		for (int i = 0; i < sizeof(cmd_char_arry); i++) { 
        		cmd_char_arry[i] = redirectCmd[i]; 
    		} 
			clientConnection->sendMsgToServer(cmd_char_arry, cmd_len, buffer, client_socket);
		}
		else {
			RDMA_LOG(INFO) << fmt::format("from redis buffer {}", from_redis);
			clientConnection->sendMsgToServer(from_redis, sizof(from_redis), buffer, client_socket);
		}
	}
	return 0;
}


AppToP2Task::~AppToP2Task()
{
}

string AppToP2Task::constructRedisReturn(string str)
{
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
