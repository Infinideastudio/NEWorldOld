#include "Network.h"
namespace Network {

	Net::Socket socketClient;
	Thread_t t;
	Mutex_t mutex;
	std::queue<Request> reqs;
	bool threadRun = true;

	void init(string ip, unsigned short _port) {
		Net::startup();

		try {
			socketClient.connectIPv4(ip, _port);
		}
		catch (...) {
			DebugError("Cannot connect to the server!");
			return;
		}

		threadRun = true;
		mutex = MutexCreate();
		t = ThreadCreate(networkThread, NULL);
	}

	size_t getRequestCount() { return reqs.size(); }
	Net::Socket& getClientSocket() { return socketClient; }

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
				Net::Buffer buffer(r._dataLen + sizeof(int) * 2);
				int len = r._dataLen + sizeof(int);
				buffer.write((void*)&len, sizeof(int));
				buffer.write((void*)&r._signal, sizeof(int));
				buffer.write((void*)r._dataSend, r._dataLen);
				getClientSocket().send(buffer);
			}
			else {
				Net::Buffer buffer(sizeof(int) * 2);
				int len = sizeof(int);
				buffer.write((void*)&len, sizeof(int));
				buffer.write((void*)&r._signal, sizeof(int));
				getClientSocket().send(buffer);
			}
			if (r._callback) { //�ж����޻ص�����
				auto callback = r._callback;
				MutexUnlock(mutex);
				int len = getClientSocket().recvInt();   //������ݳ���
				Net::Buffer buffer(len);
				getClientSocket().recv(buffer, Net::BufferConditionExactLength(len));
				if (len > 0) callback(buffer.getData(), len); //���ûص�����
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
		getClientSocket().close();
		Net::cleanup();
	}
}