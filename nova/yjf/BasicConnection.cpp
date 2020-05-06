#include "BasicConnection.h"



BasicConnection::BasicConnection(int port, int type)
{
	server_fd = 0;
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	if (type == 0) {
		address.sin_addr.s_addr = INADDR_ANY;
	}
	
	address.sin_port = htons(port);
	printf("the port is %d \n", port);
	// Forcefully attaching socket to the port 8080
	
	printf("finish initial connection \n");
}

BasicConnection::BasicConnection(char * ip, int port):BasicConnection(port, 1)
{
	this->ip = ip;
}

BasicConnection::BasicConnection(int port) : BasicConnection(port, 0)
{
}

void BasicConnection::bindAndListen()
{
	if (bind(server_fd, (struct sockaddr *) &address,
		sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	printf("----------bind port success------------- \n");
}


int BasicConnection::acceptContent()
{
	printf("begin accept content \n");
	int client_socket = 0;
	
	if ((client_socket = accept(server_fd, (struct sockaddr *) &address,
		(socklen_t *)&addrlen)) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	printf("content accepted  %d \n ", client_socket);
	return client_socket;
}

void BasicConnection::connectToServer()
{
	if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return ;
	}

	if (connect(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
		printf("\nConnection Failed \n");
		return ;
	}
}

int BasicConnection::sendMsgToServer(char* buffer, int buffer_len ,char* buffer_from_server, int socket)
{
	int from_server_len = 0;
	std::cout << "server_fd: " <<server_fd << " buffer:" << buffer << " buffer_len:" << buffer_len << std::endl;
	try {
		if (buffer_len > 0) {
			if (socket == -1) {
				send(server_fd, buffer, buffer_len, 0);
			}
			else {
				send(socket, buffer, buffer_len, 0);
			}

			from_server_len = read(server_fd, buffer_from_server, buffer_len);
			return from_server_len;
		}
		
	}
	catch (std::exception& e) {
		printf("errrno is:%d", errno);

	}
	
	return -1;
}



BasicConnection::~BasicConnection()
{
}
