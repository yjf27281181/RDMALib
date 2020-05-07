#include "P2RedirectServer.h"
#include "BasicConnection.h"


P2RedirectServer::P2RedirectServer(RDMAManager *rdmaManager)
{
	this->Pool = new CThreadPool(2);
	this->rdmaManager = rdmaManager;
}

int P2RedirectServer::Run()
{
	BasicConnection * clientConnection = new BasicConnection(8081);
	clientConnection->bindAndListen();

	while (true) {

		int client_socket = clientConnection->acceptContent();
		Pool->AddTask(new P2RedirectTask(clientConnection, client_socket, rdmaManager));
	}
	return 0;
}


P2RedirectServer::~P2RedirectServer()
{
}
