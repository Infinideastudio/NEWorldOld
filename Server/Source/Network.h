#pragma once
#include <wx/socket.h>
namespace Network{
	const unsigned short port = 30001;

	inline wxSocketServer& getServerSocket() {
		extern wxSocketServer* serverSocket;
		return *serverSocket;
	}

	void init();
}