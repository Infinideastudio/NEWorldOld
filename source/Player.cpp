﻿#include "Definitions.h"
#include "Player.h"
#include "World.h"
#include "OnlinePlayer.h"

bool Player::Glide;
bool Player::Flying;
bool Player::CrossWall;
double Player::glidingMinimumSpeed = pow(1, 2) / 2;

float Player::height = 1.2f;
float Player::heightExt = 0.0f;
bool Player::OnGround = false;
bool Player::Running = false;
bool Player::NearWall = false;
bool Player::inWater = false;
bool Player::glidingNow = false;
item Player::BlockInHand = Blocks::AIR;
ubyte Player::indexInHand = 0;

Hitbox::AABB Player::playerbox;

double Player::xa, Player::ya, Player::za, Player::xd, Player::yd, Player::zd;
double Player::health = 20, Player::healthMax = 20, Player::healSpeed = 0.01, Player::dropDamagePerBlock = 0.5;
onlineid Player::onlineID;
string Player::name;

double Player::speed;
int Player::AirJumps;
int Player::cxt, Player::cyt, Player::czt, Player::cxtl, Player::cytl, Player::cztl;
double Player::lookupdown, Player::heading, Player::xpos, Player::ypos, Player::zpos;
double Player::xposold, Player::yposold, Player::zposold, Player::jump;
double Player::xlookspeed, Player::ylookspeed;
int Player::intxpos, Player::intypos, Player::intzpos;
int Player::intxposold, Player::intyposold, Player::intzposold;

item Player::inventory[4][10];
short Player::inventoryAmount[4][10];

double Player::glidingEnergy, Player::glidingSpeed;

void InitHitbox(Hitbox::AABB& playerbox) {
	playerbox.xmin = -0.3;
	playerbox.xmax = 0.3;
	playerbox.ymin = -0.8;
	playerbox.ymax = 0.8;
	playerbox.zmin = -0.3;
	playerbox.zmax = 0.3;
}

void InitPosition() {
	Player::xposold = Player::xpos;
	Player::yposold = Player::ypos;
	Player::zposold = Player::zpos;
	Player::cxt = getchunkpos((int)Player::xpos); Player::cxtl = Player::cxt;
	Player::cyt = getchunkpos((int)Player::ypos); Player::cytl = Player::cyt;
	Player::czt = getchunkpos((int)Player::zpos); Player::cztl = Player::czt;
}

void MoveHitbox(double x, double y, double z) {
	Hitbox::MoveTo(Player::playerbox, x, y + 0.5, z);
}

void updateHitbox() {
	MoveHitbox(Player::xpos, Player::ypos, Player::zpos);
}

void Player::init(double x, double y, double z) {
	xpos = x;
	ypos = y;
	zpos = z;
	InitHitbox(Player::playerbox);
	InitPosition();
	updateHitbox();
}

void Player::spawn() {
	xpos = 0;
	ypos = 60;
	zpos = 0;
	jump = 0;
	InitHitbox(Player::playerbox);
	InitPosition();
	updateHitbox();
	memset(inventory, 0, sizeof(inventory));
	memset(inventoryAmount, 0, sizeof(inventoryAmount));
	inventory[0][0] = 1; inventoryAmount[0][0] = 255;
	inventory[0][1] = 2; inventoryAmount[0][1] = 255;
	inventory[0][2] = 3; inventoryAmount[0][2] = 255;
	inventory[0][3] = 4; inventoryAmount[0][3] = 255;
	inventory[0][4] = 5; inventoryAmount[0][4] = 255;
	inventory[0][5] = 6; inventoryAmount[0][5] = 255;
	inventory[0][6] = 7; inventoryAmount[0][6] = 255;
	inventory[0][7] = 8; inventoryAmount[0][7] = 255;
	inventory[0][8] = 9; inventoryAmount[0][8] = 255;
	inventory[0][9] = 10; inventoryAmount[0][9] = 255;
	inventory[1][0] = 11; inventoryAmount[1][0] = 255;
	inventory[1][1] = 12; inventoryAmount[1][1] = 255;
	inventory[1][2] = 13; inventoryAmount[1][2] = 255;
	inventory[1][3] = 14; inventoryAmount[1][3] = 255;
	inventory[1][4] = 15; inventoryAmount[1][4] = 255;
	inventory[1][5] = 16; inventoryAmount[1][5] = 255;
	inventory[1][6] = 17; inventoryAmount[1][6] = 255;
	inventory[1][7] = 18; inventoryAmount[1][7] = 255;
}

void Player::updatePosition() {

	inWater = World::inWater(playerbox);
	if (!Flying && !CrossWall && inWater) {
		xa *= 0.6;
		ya *= 0.6;
		za *= 0.6;
	}

	double xal = xa;
	double yal = ya;
	double zal = za;
	static double ydam = 0;
	if (!CrossWall) {
		vector<Hitbox::AABB> Hitboxes = World::getHitboxes(Hitbox::Expand(playerbox, xa, ya, za));
		int num = Hitboxes.size();
		if (num > 0) {
			for (int i = 0; i < num; i++) {
				ya = Hitbox::MaxMoveOnYclip(playerbox, Hitboxes[i], ya);
			}
			Hitbox::Move(playerbox, 0.0, ya, 0.0);
			for (int i = 0; i < num; i++) {
				xa = Hitbox::MaxMoveOnXclip(playerbox, Hitboxes[i], xa);
			}
			Hitbox::Move(playerbox, xa, 0.0, 0.0);
			for (int i = 0; i < num; i++) {
				za = Hitbox::MaxMoveOnZclip(playerbox, Hitboxes[i], za);
			}
			Hitbox::Move(playerbox, 0.0, 0.0, za);
		}
		if (!Flying) {
			if (ypos + ya > ydam) ydam = ypos + ya;
		}
		if (ya != yal && yal < 0.0) {
			OnGround = true;
			Player::glidingEnergy = 0;
			Player::glidingSpeed = 0;
			Player::glidingNow = false;
			if (ydam - (ypos + ya) > 0) {
				Player::health -= (ydam - (ypos + ya)) * Player::dropDamagePerBlock;
				ydam = 0;
			}
		}
		else OnGround = false;
		if (ya != yal && yal>0.0) jump = 0.0;
		if (xa != xal || za != zal) NearWall = true; else NearWall = false;
		xd = xa; yd = ya; zd = za;
		xpos += xa; ypos += ya; zpos += za;
		xa *= 0.8;
		za *= 0.8;
		if (OnGround) {
			xa *= 0.7;
			za *= 0.7;
		}
		if (Flying) ya *= 0.8;
	}
	else {
		xpos += xa;
		ypos += ya;
		zpos += za;

		xa *= 0.8;
		ya *= 0.8;
		za *= 0.8;
	}

	cxtl = cxt; cytl = cyt; cztl = czt;
	cxt = getchunkpos((int)xpos);
	cyt = getchunkpos((int)ypos);
	czt = getchunkpos((int)zpos);

	updateHitbox();
}

bool Player::putBlock(int x, int y, int z, block blockname) {
	Hitbox::AABB blockbox;
	bool success = false;
	blockbox.xmin = x - 0.5;
	blockbox.xmax = x + 0.5;
	blockbox.ymin = y - 0.5;
	blockbox.ymax = y + 0.5;
	blockbox.zmin = z - 0.5;
	blockbox.zmax = z + 0.5;
	if (((Hitbox::Hit(playerbox, blockbox) == false) || CrossWall || BlockInfo(blockname).isSolid() == false) && BlockInfo(World::getblock(x, y, z)).isSolid() == false) {
		World::putblock(x, y, z, blockname);
		success = true;
	}
	return success;
}

bool Player::save(string worldn) {
	uint32 curversion = VERSION;
	std::ofstream isave("Worlds/" + worldn + "/player.NEWorldPlayer", std::ios::binary | std::ios::out);
	if (!isave.is_open()) return false;
	isave.write((char*)&curversion, sizeof(curversion));
	isave.write((char*)&xpos, sizeof(xpos));
	isave.write((char*)&ypos, sizeof(ypos));
	isave.write((char*)&zpos, sizeof(zpos));
	isave.write((char*)&lookupdown, sizeof(lookupdown));
	isave.write((char*)&heading, sizeof(heading));
	isave.write((char*)&jump, sizeof(jump));
	isave.write((char*)&OnGround, sizeof(OnGround));
	isave.write((char*)&Running, sizeof(Running));
	isave.write((char*)&AirJumps, sizeof(AirJumps));
	isave.write((char*)&Flying, sizeof(Flying));
	isave.write((char*)&CrossWall, sizeof(CrossWall));
	isave.write((char*)&indexInHand, sizeof(indexInHand));
	isave.write((char*)&health, sizeof(health));
	isave.write((char*)inventory, sizeof(inventory));
	isave.write((char*)inventoryAmount, sizeof(inventoryAmount));
	isave.close();
	return true;
}

bool Player::load(string worldn) {
	uint32 targetVersion;
	std::ifstream iload("Worlds/" + worldn + "/player.NEWorldPlayer", std::ios::binary | std::ios::in);
	if (!iload.is_open()) return false;
	iload.read((char*)&targetVersion, sizeof(targetVersion));
	if (targetVersion != VERSION) return false;
	iload.read((char*)&xpos, sizeof(xpos));
	iload.read((char*)&ypos, sizeof(ypos));
	iload.read((char*)&zpos, sizeof(zpos));
	iload.read((char*)&lookupdown, sizeof(lookupdown));
	iload.read((char*)&heading, sizeof(heading));
	iload.read((char*)&jump, sizeof(jump));
	iload.read((char*)&OnGround, sizeof(OnGround));
	iload.read((char*)&Running, sizeof(Running));
	iload.read((char*)&AirJumps, sizeof(AirJumps));
	iload.read((char*)&Flying, sizeof(Flying));
	iload.read((char*)&CrossWall, sizeof(CrossWall));
	iload.read((char*)&indexInHand, sizeof(indexInHand));
	iload.read((char*)&health, sizeof(health));
	iload.read((char*)inventory, sizeof(inventory));
	iload.read((char*)inventoryAmount, sizeof(inventoryAmount));
	iload.close();
	return true;
}

void Player::addItem(item itemname, short amount) {
	//向背包里加入物品
	const int InvMaxStack = 255;
	for (int i = 3; i >= 0; i--) {
		for (int j = 0; j != 10; j++) {
			if (inventory[i][j] == itemname && inventoryAmount[i][j] < InvMaxStack) {
				//找到一个同类格子
				if (amount + inventoryAmount[i][j] <= InvMaxStack) {
					inventoryAmount[i][j] += amount;
					return;
				}
				else {
					amount -= InvMaxStack - inventoryAmount[i][j];
					inventoryAmount[i][j] = InvMaxStack;
				}
			}
			else if (inventory[i][j] == Blocks::AIR) {
				//找到一个空白格子
				inventory[i][j] = itemname;
				if (amount <= InvMaxStack) {
					inventoryAmount[i][j] = amount;
					return;
				}
				else {
					inventoryAmount[i][j] = InvMaxStack;
					amount -= InvMaxStack;
				}
			}
		}
	}
}

PlayerPacket Player::convertToPlayerPacket()
{
	PlayerPacket p;
	p.x = xpos;
	p.y = ypos + height + heightExt;
	p.z = zpos;
	p.heading = heading;
	p.lookupdown = lookupdown;
	p.onlineID = onlineID;
	p.skinID = 0;
	strcpy(p.name, name.c_str());
	return p;
}

