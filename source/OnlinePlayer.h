#pragma once
#include "Object.h"
#include "stdinclude.h"

extern map<SkinID, pair<VBOID, vtxCount>> playerSkins;

struct PlayerPacket {
	double x, y, z;
	char name[64];
	int onlineID;
	int skinID;
};

class OnlinePlayer : public Object {
public:
	OnlinePlayer(double x, double y, double z, string name, int onlineID, SkinID skinID) :
		Object(x, y, z), _name(name), _onlineID(onlineID), _skinID(skinID)
	{
		auto iter = playerSkins.find(_skinID);
		if (iter != playerSkins.end()) {
			VBO = iter->second.first;
			vtxs = iter->second.second;
		}
		else {
			VBO = 0;
			vtxs = 0;
			GenVAOVBO(_skinID); //生成玩家的VAO/VBO
			playerSkins[_skinID] = std::make_pair(VBO, vtxs);
		}
	};

	const string& getName() const { return _name; }

	static OnlinePlayer convertFromPlayerPacket(PlayerPacket p) {
		return OnlinePlayer(p.x, p.y, p.z, p.name, p.onlineID, p.skinID);
	}

	void GenVAOVBO(int skinID);

	void render();

private:
	const string _name;
	int _onlineID;
	SkinID _skinID;

};