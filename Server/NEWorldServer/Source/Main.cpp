#include <vector>
#include <thread>
#include <mutex>
#include <Winsock2.h>
#pragma comment( lib, "ws2_32.lib" )
//#include "..\..\..\source\OnlinePlayer.h"

SOCKET init() {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN addrSrv;
	addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(25565);
	bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
	listen(sockSrv, 5);
	return sockSrv;
}

void handle(SOCKET sockConn) {
	closesocket(sockConn);
}

int main() {
	//≥ı ºªØwinsocket  
	SOCKET sockSrv = init();

	SOCKADDR_IN addrClient;
	int len = sizeof(SOCKADDR);
	std::vector<std::thread> sockets;

	while (true) {
		SOCKET sockConn = accept(sockSrv, (SOCKADDR*)&addrClient, &len);
		sockets.push_back(std::thread(handle, sockConn));
	}
	WSACleanup();
	system("pause");
	return 0;
}