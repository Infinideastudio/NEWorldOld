#include "Definitions.h"
#include "Network.h"
namespace Network {

	wxSocketClient* socketClient;
	Thread_t t;
	Mutex_t mutex;
	std::queue<Request> reqs;
	bool threadRun = true;

	void init(string ip, unsigned short _port) {
		socketClient = new wxSocketClient;
		wxIPV4address addr;
		addr.Hostname(ip);
		addr.Service(_port);
		socketClient->Connect(addr);

		threadRun = true;
		mutex = MutexCreate();
		t = ThreadCreate(networkThread, NULL);
	}

	int getRequestCount() { return reqs.size(); }
	wxSocketClient* getClientSocket() { return socketClient; }

	ThreadFunc networkThread(void*) {
		while (updateThreadRun) {
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
				getClientSocket()->Write((const void*)&r._signal, sizeof(int));
				getClientSocket()->Write((const void*)r._dataSend, r._dataLen);
			}
			else {
				getClientSocket()->Write((const void*)&r._signal, sizeof(int));
			}
			if (r._callback) { //判断有无回调函数
				auto callback = r._callback;
				MutexUnlock(mutex);
				int len;
				getClientSocket()->Read(&len, sizeof(int));   //获得数据长度
				char* buffer = new char[len];
				getClientSocket()->Read(buffer,len);
				if (len > 0) callback(buffer, len); //调用回调函数
				delete[] buffer;
				MutexLock(mutex);
			}
			reqs.pop();
			MutexUnlock(mutex);
		}
		return 0;
	}

	void pushRequest(Request& r) {
		if (reqs.size() + 1 > networkRequestMax) {  //超过请求队列长度，试图精简队列
			if (!reqs.front().isImportant()) reqs.pop();
		}
		if (reqs.size() + 1 > networkRequestMax * 2) {  //大量超过请求队列长度，只保留重要请求
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
		socketClient->Destroy();
	}
}