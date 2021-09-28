#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <process.h>

#include <iostream>
#include <map>
#include <string>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

struct transferStruct {
	int type, length;
	char data[1024];
};

struct Client {
	int id;
	std::string name;
	SOCKET socket;
	Client() {
		id = 0;
		name = "Anonymous";
		socket = INVALID_SOCKET;
	}
};

std::map<int, Client> clients;

const char* welcome_str = "Welcome";
const int welcome_len = strlen(welcome_str);
int conn_number = 0;

void _basic_send_message(SOCKET s,const char* buf, int type = 0, int length = 0) {
	transferStruct data;
	if (type != 6 && type != 7) strcpy_s(data.data, sizeof(data.data), buf);
	else memcpy_s(data.data, 1024, buf, length);
	data.type = type; data.length = length;
	send(s, (const char*)&data, sizeof(data), 0);
}

unsigned __stdcall client_run(void* args) {
	Client* client = (Client*)args;
	if (client->socket == INVALID_SOCKET) {
		puts("Accept error!");
		closesocket(client->socket);
	}
	//send(client->socket, welcome_str, welcome_len, 0);
	_basic_send_message(client->socket, welcome_str);
	puts("New thread started.");
	transferStruct recvData;
	while (true) {
		int ret = recv(client->socket, (char*)&recvData, sizeof(recvData), 0);//监听消息
		if (ret < 0) break;//断开连接
		std::string tmp = ""; // = recvData.data;
		printf("Forward message type: %d\n", recvData.type);
		if (recvData.type == 1 || recvData.type == 0) {
			time_t now = time(0);
			tm* ltm = localtime(&now);
			char t[50];
			sprintf_s(t, "<%02d:%02d:%02d>", ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
			tmp = t;
			tmp += client->name + ":" + recvData.data;
			strcpy_s(recvData.data, sizeof(recvData.data), tmp.c_str());
		}
		else if (recvData.type == 4) {
			client->name = recvData.data;
			continue;
		}
		for (auto iter = clients.begin(); iter != clients.end(); ++iter) {//群发
			_basic_send_message(iter->second.socket, recvData.data, recvData.type, recvData.length);
		}
	}
	clients.erase(client->id);
	conn_number--;
	closesocket(client->socket);
	printf("一个连接断开线程执行完毕\n");
	return 0;
}

int main(void)
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo* result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	Client* client;
	int id = 0;
	// Accept a client socket
	while (true) {
		client = new Client();
		client->socket = accept(ListenSocket, NULL, NULL);
		// send(client->socket, welcome_str, welcome_len, 0);
		if (client->socket < 0) {
			delete client;
			puts("接收失败");
			continue;
		}
		client->id = id;
		clients[id] = *client;
		conn_number++;
		int ret = _beginthreadex(NULL, 0, client_run, &clients[id], 0, 0);
		id++;
	}

	WSACleanup();

	return 0;
}