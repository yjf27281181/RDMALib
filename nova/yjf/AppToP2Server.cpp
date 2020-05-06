#include "AppToP2Server.h"
#include "BasicConnection.h"


AppToP2Server::AppToP2Server(RDMAManager *rdmaManager)
{
	this->Pool = new CThreadPool(10);
	this->rdmaManager = rdmaManager;
}

int AppToP2Server::Run()
{
	BasicConnection * clientConnection = new BasicConnection(8080);
	clientConnection->bindAndListen();
	while (true) {
		int client_socket = clientConnection->acceptContent();
		Pool->AddTask(new AppToP2Task(clientConnection, client_socket, rdmaManager));
	}
	return 0;
}


AppToP2Server::~AppToP2Server()
{
}
