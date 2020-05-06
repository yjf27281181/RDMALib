#pragma once
#include "P2RedirectTask.h"
#include "Thread.h"
#include "../rdma_manager.h"
class P2RedirectServer : public CTask
{
public:
	P2RedirectServer(RDMAManager *rdmaManager);
	int Run();
	~P2RedirectServer();
private:
	CThreadPool* Pool;
	RDMAManager *rdmaManager;
};