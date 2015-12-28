#ifndef PLAYERPACKET_H
#define PLAYERPACKET_H
enum { PLAYER_PACKET_SEND, PLAYER_PACKET_REQ };
struct PlayerPacket {
	double x, y, z;
	double lookupdown, heading;
	char name[32];
	int onlineID;
	int skinID;
};
#endif