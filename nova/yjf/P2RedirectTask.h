#pragma once
#include "Thread.h"
#include "BasicConnection.h"
#include <string>
#include <iostream>
#include <vector>
#include "../rdma_manager.h"
using namespace std;

class P2RedirectTask : public CTask
{
public:
	P2RedirectTask(BasicConnection* clientConnection, int client_socket, RDMAManager *rdmaManager, mutex* allocateMemMutex);
	int Run();
	~P2RedirectTask();
private:
	BasicConnection* clientConnection;
	char buffer[2048];
	int from_app_len;
	int client_socket;
	string constructRedisReturn(string str);
	RDMAManager *rdmaManager;
	mutex* allocateMemMutex;
};

