#define NEWORLD_SERVER
#include <thread>
#include <mutex>
#include <algorithm>
#include <vector>
#include <map>
#include "Network.h"
#include "Console.h"
#include "..\..\..\source\PlayerPacket.h"
using std::map;
using std::vector;
using std::thread;

map<int, PlayerPacket> players;
std::mutex m;

void handle(SOCKET sockConn) {
	char receiveBuf[128]; //���ջ�����
	unsigned int onlineID = 0;
	bool IDSet = false;
	enum { PLAYER_PACKET_SEND, PLAYER_PACKET_REQ };
	Print("New connection.");
	while (true) {
		int recvbyte = recv(sockConn, receiveBuf, sizeof(receiveBuf), 0);
		if (recvbyte == 0 || recvbyte == -1) {
			Print("A connection closed.");
			closesocket(sockConn);
			m.lock();
			players.erase(onlineID);
			m.unlock();
			return;
		}
		int signal = *receiveBuf; //ǿ�ƽضϣ���ȡsignal
		char* data = receiveBuf + sizeof(int);
		m.lock();
		switch (signal) {
		case PLAYER_PACKET_SEND:
		{
			//�ͻ���������ݸ���
			PlayerPacket* pp = (PlayerPacket*)data;
			map<int, PlayerPacket>::iterator iter = players.find(pp->onlineID);
			if (iter == players.end()) {
				players[pp->onlineID] = *pp;  //��һ���ϴ�����
				IDSet = true;
				onlineID = pp->onlineID;
			}
			else {
				iter->second = *pp;  //��������
			}
			break;
		}
		case PLAYER_PACKET_REQ:
		{
			//�ͻ�������������ҵ�λ��
			if (players.size() == 0) break;
			PlayerPacket* playersData = new PlayerPacket[players.size()];
			int i = 0;
			for (auto iter = players.begin(); iter != players.end(); ++iter, ++i) playersData[i] = iter->second;
			send(sockConn, (const char*)playersData, players.size()*sizeof(PlayerPacket), 0);
			break;
		}
		}
		m.unlock();
	}
}

int main() {
	Print("The server is starting...");
	vector<thread> clients;
	while (true) {
		clients.push_back(thread(handle, Network::waitForClient()));
	}
	Print("The server is stopping...");
	std::for_each(clients.begin(), clients.end(), [](thread& t) {t.join(); });
	Network::cleanUp();
	system("pause");
	return 0;
}