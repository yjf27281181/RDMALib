#include "P2RedirectServer.h"
#include "BasicConnection.h"


P2RedirectServer::P2RedirectServer()
{
	this->Pool = new CThreadPool(10);
}

int P2RedirectServer::Run()
{
	char buffer[1024] = { 0 }, from_redis[1024] = { 0 };
	BasicConnection * clientConnection = new BasicConnection(8081);
	clientConnection->bindAndListen();

	while (true) {

		int client_socket = clientConnection->acceptContent();
		int from_app_len = read(client_socket, buffer, 1024);
		printf("chakan buffer content %s", buffer);
		Pool->AddTask(new P2RedirectTask(clientConnection, buffer, from_app_len, client_socket));
	}
	return 0;
}


P2RedirectServer::~P2RedirectServer()
{
}
