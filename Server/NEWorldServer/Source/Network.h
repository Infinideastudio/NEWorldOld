#pragma once
//Netycat
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "..\..\..\include\Netycat\include\Netycat\Netycat.h"
namespace Net = Netycat::Core;
#pragma comment(lib, "ws2_32.lib")

namespace Network{
	const unsigned short port = 30001;

	inline Net::Socket& getServerSocket() {
		extern Net::Socket serverSocket;
		return serverSocket;
	}

	void cleanUp();

	void init();
}