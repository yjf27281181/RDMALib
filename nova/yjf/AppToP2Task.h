#pragma once
#include <string>
#include "Thread.h"
#include "BasicConnection.h"
#include <vector>
#include <iostream>
#include "../rdma_manager.h"
using namespace std;
class AppToP2Task : public CTask
{
public:
	AppToP2Task(BasicConnection* clientConnection, int client_socket, RDMAManager *rdmaManager);
	int Run();
	~AppToP2Task();

private:
	BasicConnection* clientConnection;
	char buffer[2048];
	int from_app_len;
	int client_socket;
	string constructStringReturn(string str);
	string constructMapReturn(string str);
	RDMAManager *rdmaManager;
};

