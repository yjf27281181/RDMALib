#pragma once
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
class BasicConnection
{
public:
	BasicConnection(int port);
	//建立p2 - p2发送端 或者 p2 到 redis的发送端
	BasicConnection(int port, int ip);
	//建立p2 - p2接受端 或者建立p2与application的接收端
	BasicConnection(char* ip, int port);

	//绑定接口并进行监听 
	void bindAndListen();
	//准备接受消息
	int acceptContent(); //return client_socket

	//通过ip连接远程server（p2或者reids）
	void connectToServer();
	//向另一端发送数据，socket == -1 表示是p2向reids发送请求，socket有值表示p2向已建立链接的socket发送请求
	int sendMsgToServer(char* buffer, int buffer_len, char* buffer_from_server, int socket); //return length of msg
	~BasicConnection();

private:
	struct sockaddr_in address;
	int opt = 1;
	int addrlen;
	int server_fd;
	char* ip;
};

