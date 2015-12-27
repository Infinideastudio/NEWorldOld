#include "Network.h"

namespace Network {
	wxSocketServer* serverSocket;
	void init() {
		wxIPV4address addr;
		addr.Service(port);
		serverSocket = new wxSocketServer(addr);
		if (!serverSocket->Ok())
		{
			delete serverSocket;
			serverSocket = nullptr;
			return;
		}
	}
}