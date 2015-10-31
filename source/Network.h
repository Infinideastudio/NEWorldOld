#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <functional>
#include "Definitions.h"

namespace Network{
	enum { PLAYER_PACKET_SEND, PLAYER_PACKET_REQ };
	class Request {
	public:
		Request(const char* dataSend, int dataLen, int signal) :
			_dataSend(dataSend), _dataLen(dataLen), _signal(signal) {};
		Request(const char* dataSend, int dataLen, int signal, std::function<void(void*, int)> callback) :
			_dataSend(dataSend), _dataLen(dataLen), _signal(signal), _callback(callback) {};
		friend ThreadFunc networkThread(void*);
	private:
		int _signal;
		const char* _dataSend;
		int _dataLen;
		std::function<void(void*, int)> _callback;
	};

	extern Mutex_t mutex;

	const unsigned short port = 30001;

	void init(string ip);

	int getRequestCount();

	SOCKET getClientSocket();

	ThreadFunc networkThread(void*);

	void pushRequest(Request& r);

	void cleanUp();
}