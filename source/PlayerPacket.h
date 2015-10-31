#pragma once
struct PlayerPacket {
	double x, y, z;
	double lookupdown, heading;
	char name[32];
	int onlineID;
	int skinID;
};