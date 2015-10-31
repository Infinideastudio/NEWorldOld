#include "Network.h"

namespace Network {
	SOCKET getServerSocket() {
		static SOCKET serverSocket;
		static bool firstCall = true;
		if (firstCall) {
			WSADATA wsaData;
			WSAStartup(MAKEWORD(1, 1), &wsaData);
			serverSocket = socket(AF_INET, SOCK_STREAM, 0);
			SOCKADDR_IN addrSrv;
			addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
			addrSrv.sin_family = AF_INET;
			addrSrv.sin_port = htons(port);
			bind(serverSocket, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
			listen(serverSocket, 5);
			firstCall = false;
		}
		return serverSocket;
	}
	SOCKET waitForClient() {
		static SOCKADDR_IN addrClient;
		static int len = sizeof(SOCKADDR);
		return accept(Network::getServerSocket(), (SOCKADDR*)&addrClient, &len);
	}
	void cleanUp() {
		WSACleanup();
	}
}