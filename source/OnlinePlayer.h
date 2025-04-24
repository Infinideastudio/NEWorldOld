#pragma once
#include "Object.h"
#include "stdinclude.h"
#include "PlayerPacket.h"

extern map<SkinID, Renderer::VertexBuffer> playerSkins;

class OnlinePlayer : public Object {
public:
	OnlinePlayer(double x, double y, double z, string name, OnlineID onlineID, SkinID skinID, double lookupdown, double heading) :
		Object(x, y, z, genVertexBuffer(skinID)), _name(name), _onlineID(onlineID), _skinID(skinID), _lookupdown(lookupdown), _heading(heading) {}
	
	OnlinePlayer(PlayerPacket& p) :
		OnlinePlayer(p.x, p.y, p.z, p.name, p.onlineID, p.skinID, p.lookupdown, p.heading) {}

	const string& getName() const { return _name; }

	const OnlineID getOnlineID() const { return _onlineID; }

	void render() const;

private:
	string _name;
	OnlineID _onlineID;
	SkinID _skinID;
	double _lookupdown, _heading;

	static Renderer::VertexBuffer genVertexBuffer(SkinID skinID);
};

extern vector<OnlinePlayer> players;