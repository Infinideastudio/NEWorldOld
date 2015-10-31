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
	char receiveBuf[128]; //接收缓存区
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
		int signal = *receiveBuf; //强制截断，提取signal
		char* data = receiveBuf + sizeof(int);
		m.lock();
		switch (signal) {
		case PLAYER_PACKET_SEND:
		{
			//客户端玩家数据更新
			PlayerPacket* pp = (PlayerPacket*)data;
			map<int, PlayerPacket>::iterator iter = players.find(pp->onlineID);
			if (iter == players.end()) {
				players[pp->onlineID] = *pp;  //第一次上传数据
				IDSet = true;
				onlineID = pp->onlineID;
			}
			else {
				iter->second = *pp;  //更新数据
			}
			break;
		}
		case PLAYER_PACKET_REQ:
		{
			//客户端请求其他玩家的位置
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