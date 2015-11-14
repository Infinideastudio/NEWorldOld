#include "Network.h"
#include <queue>
namespace Network {

	SOCKET sockClient;
	Thread_t t;
	Mutex_t mutex;
	std::queue<Request> reqs;
	bool threadRun = true;

	void init(string ip, unsigned short _port) {
		WSADATA wsaData;
		WSAStartup(MAKEWORD(1, 1), &wsaData);
		sockClient = socket(AF_INET, SOCK_STREAM, 0);
		SOCKADDR_IN addrSrv;
		addrSrv.sin_addr.S_un.S_addr = inet_addr(ip.c_str());
		addrSrv.sin_family = AF_INET;
		addrSrv.sin_port = htons(_port);
		connect(sockClient, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));

		threadRun = true;
		mutex = MutexCreate();
		t = ThreadCreate(networkThread, NULL);
	}

	int getRequestCount() { return reqs.size(); }

	SOCKET getClientSocket() {
		return sockClient;
	}

	ThreadFunc networkThread(void *) {
		while (true) {
			MutexLock(mutex);
			if (!threadRun) {
				MutexUnlock(mutex);
				break;
			}
			if (reqs.empty()) {
				MutexUnlock(mutex);
				continue;
			}
			Request& r = reqs.front();
			//if (r._signal == PLAYER_PACKET_SEND && ((PlayerPacket*)r._dataSend)->onlineID != player::onlineID)
			//	cout << "[ERROR]WTF!!!" << endl;
			if (r._dataSend != nullptr && r._dataLen != 0) {
				char* data = new char[r._dataLen + sizeof(int)];
				int* signal = (int*)data;
				*signal = r._signal;
				memcpy_s(data + sizeof(int), r._dataLen, r._dataSend, r._dataLen);
				send(getClientSocket(), data, r._dataLen + sizeof(int), 0);
				delete[] data;
			}
			else {
				send(getClientSocket(), (const char*)&r._signal, sizeof(int), 0);
			}
			if (r._callback) { //�ж����޻ص�����
				auto callback = r._callback;
				MutexUnlock(mutex);
				char recvBuf[1024]; //���ջ�����
				int len = recv(sockClient, recvBuf, 1024, 0); //������ݳ���
				if (len > 0) callback(recvBuf, len); //���ûص�����
				MutexLock(mutex);
			}
			reqs.pop();
			MutexUnlock(mutex);
		}
		return 0;
	}

	void pushRequest(Request& r) {
		if (reqs.size() + 1 > networkRequestMax) {  //����������г��ȣ���ͼ�������
			if (!reqs.front().isImportant()) reqs.pop();
		}
		if (reqs.size() + 1 > networkRequestMax * 2) {  //��������������г��ȣ�ֻ������Ҫ����
			std::queue<Request> q;
			while (reqs.size() != 0) {
				if (reqs.front().isImportant()) q.push(reqs.front());
				reqs.pop();
			}
			reqs = q;
		}
		reqs.push(r);
	}
	
	void cleanUp() {
		threadRun = false;
		ThreadWait(t);
		ThreadDestroy(t);
		MutexDestroy(mutex);
		closesocket(getClientSocket());
		WSACleanup();
	}
}