#pragma once
#include "AppToP2Task.h"
#include "Thread.h"
#include "AppToP2Task.h"
class AppToP2Server : public CTask
{
public:
	AppToP2Server(RDMAManager *rdmaManager);
	int Run();
	~AppToP2Server();
private:
	CThreadPool* Pool;
	RDMAManager *rdmaManager;
};

