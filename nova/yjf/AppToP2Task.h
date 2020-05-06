#pragma once
#include <string>
#include "Thread.h"
#include "BasicConnection.h"
#include <vector>
#include <iostream>
#include "AppToP2Task.h"
using namespace std;
class AppToP2Task : public CTask
{
public:
	AppToP2Task(BasicConnection* clientConnection, int client_socket, RDMAManager *rdmaManager);
	int Run();
	~AppToP2Task();

private:
	BasicConnection* clientConnection;
	char buffer[1024] = { 0 };
	int from_app_len;
	int client_socket;
	char* constructRedisReturn(string str);
	RDMAManager *rdmaManager;
};
