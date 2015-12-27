#define NEWORLD_SERVER
#include <vector>
#include <map>
#include "Network.h"
#include "Console.h"
#include "..\..\..\source\PlayerPacket.h"
#include <pthread/pthread.h>
#include <wx/init.h>
using std::map;
using std::vector;

map<int, PlayerPacket> players;
pthread_mutex_t* m;

void* __cdecl handle(void* _) {
	wxSocketBase* socket = (wxSocketBase*)_;
	unsigned int onlineID = 0;
	bool IDSet = false;
	pthread_mutex_lock(m);
	Print("New connection. Online players:" + toString(players.size()+1));
	pthread_mutex_unlock(m);
	while (true) {
		
		int len = -1;
		socket->Read(&len, 4);	//获得数据长度
		if (len == -1) {
			Print("A connection closed.");
			socket->Destroy();
			pthread_mutex_lock(m);
			players.erase(onlineID);
			pthread_mutex_unlock(m);
			return nullptr;
		}

		unsigned char* buffer = new unsigned char[len];
		socket->Read(buffer, len);

		int signal;
		socket->Read(&signal, 4);
		char* data = (char*)buffer + sizeof(int);
		pthread_mutex_lock(m);
		Print("Online players:" + toString(players.size()));
		switch (signal) {
		case PLAYER_PACKET_SEND:
		{
			//客户端玩家数据更新
			PlayerPacket* pp = (PlayerPacket*)data;
			if (IDSet&&pp->onlineID != onlineID) {
				Print("The packet is trying to change other player's data. May cheat? (Packet from " + toString(onlineID) + ")", MESSAGE_WARNING);
				break;
			}
			map<int, PlayerPacket>::iterator iter = players.find(pp->onlineID);
			if (iter == players.end()) {
				if (IDSet) {
					Print("Can't find player data, may change the online id in game. (" + toString(onlineID) + " to " + toString(pp->onlineID) + ")", MESSAGE_WARNING);
					break;
				}
				players[pp->onlineID] = *pp;  //第一次上传数据
				IDSet = true;
				onlineID = pp->onlineID;
			}
			else {
				if (!IDSet) {
					Print("May repeat login?", MESSAGE_WARNING);
					break;
				}
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
			for (auto iter = players.begin(); iter != players.end(); ++iter) {
				playersData[i] = iter->second;
				i++;
			}
			socket->Write(playersData, players.size()*sizeof(PlayerPacket));
			break;
		}
		}
		pthread_mutex_unlock(m);
		delete[] buffer;
	}
	return nullptr;
}

int main() {
	wxInitialize();
	Print("NEWorld Server 0.2.1(Dev.) for NEWorld Alpha 0.5.0(Dev.). Using the developing version to play is not recommended.");
	Print("The server is starting...");
	Network::init();
	Print("The server is running.");
	std::vector<pthread_t> clients;
	m = new pthread_mutex_t;
	pthread_mutex_init(m, nullptr);
	while (true) {
		wxSocketBase* socketAccept;
		socketAccept = Network::getServerSocket().Accept();
		pthread_t thread;
		pthread_create(&thread, nullptr, handle, socketAccept);
		clients.push_back(thread);
	}
	Print("The server is stopping...");
	for (std::vector<pthread_t>::iterator it = clients.begin(); it != clients.end(); it++)
		pthread_join(*it, nullptr);
	delete m;
	system("pause");
	wxUninitialize();
	return 0;
}