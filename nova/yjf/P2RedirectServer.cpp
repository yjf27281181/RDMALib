#include "P2RedirectServer.h"
#include "BasicConnection.h"


P2RedirectServer::P2RedirectServer(RDMAManager *rdmaManager)
{
	this->Pool = new CThreadPool(10);
	this->rdmaManager = rdmaManager
}

int P2RedirectServer::Run()
{
	char buffer[1024] = { 0 }, from_redis[1024] = { 0 };
	BasicConnection * clientConnection = new BasicConnection(8081);
	clientConnection->bindAndListen();

	while (true) {

		int client_socket = clientConnection->acceptContent();
		int from_app_len = read(client_socket, buffer, 1024);
		Pool->AddTask(new P2RedirectTask(clientConnection, buffer, from_app_len, client_socket, rdmaManager));
	}
	return 0;
}


P2RedirectServer::~P2RedirectServer()
{
}
