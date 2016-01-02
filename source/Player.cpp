#include "Player.h"
#include "World.h"

bool canGliding = false; //����
bool FLY;      //����
bool CROSS;    //��ǽ ��_�� (Superman!)
double glidingMinimumSpeed = pow(1, 2) / 2;

float player::height = 1.2f;
float player::heightExt = 0.0f;
bool player::OnGround = false;
bool player::Running = false;
bool player::NearWall = false;
bool player::inWater = false;
bool player::glidingNow = false;
item player::BlockInHand = blocks::AIR;
ubyte player::indexInHand = 0;

Hitbox::AABB player::playerbox;

double player::xa, player::ya, player::za, player::xd, player::yd, player::zd;
double player::health = 20, player::healthMax = 20, player::healSpeed = 0.01, player::dropDamagePerBlock = 0.5;
onlineid player::onlineID;
string player::name;

double player::speed;
int player::AirJumps;
int player::cxt, player::cyt, player::czt, player::cxtl, player::cytl, player::cztl;
double player::lookupdown, player::heading, player::xpos, player::ypos, player::zpos;
double player::xposold, player::yposold, player::zposold, player::jump;
double player::xlookspeed, player::ylookspeed;
int player::intxpos, player::intypos, player::intzpos;
int player::intxposold, player::intyposold, player::intzposold;

item player::inventory[4][10];
item player::inventoryAmount[4][10];

double player::glidingEnergy, player::glidingSpeed;

void InitHitbox(Hitbox::AABB& playerbox) {
	playerbox.xmin = -0.3;
	playerbox.xmax = 0.3;
	playerbox.ymin = -0.8;
	playerbox.ymax = 0.8;
	playerbox.zmin = -0.3;
	playerbox.zmax = 0.3;
}

void InitPosition() {
	player::xposold = player::xpos;
	player::yposold = player::ypos;
	player::zposold = player::zpos;
	player::cxt = getchunkpos((int)player::xpos); player::cxtl = player::cxt;
	player::cyt = getchunkpos((int)player::ypos); player::cytl = player::cyt;
	player::czt = getchunkpos((int)player::zpos); player::cztl = player::czt;
}

void MoveHitbox(double x, double y, double z) {
	Hitbox::MoveTo(player::playerbox, x, y + 0.5, z);
}

void updateHitbox() {
	MoveHitbox(player::xpos, player::ypos, player::zpos);
}

void player::init(double x, double y, double z)
{
	xpos = x;
	ypos = y;
	zpos = z;
	InitHitbox(player::playerbox);
	InitPosition();
	updateHitbox();
}

void player::updatePosition() {

	inWater = world::inWater(playerbox);
	if (!FLY && !CROSS && inWater) {
		xa *= 0.6;
		ya *= 0.6;
		za *= 0.6;
	}

	double xal = xa;
	double yal = ya;
	double zal = za;
	static double ydam = 0;
	if (!CROSS) {
		vector<Hitbox::AABB> Hitboxes = world::getHitboxes(Hitbox::Expand(playerbox, xa, ya, za));
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
		if (!FLY) {
			if (ypos + ya > ydam) ydam = ypos + ya;
		}
		if (ya != yal && yal < 0.0) {
			OnGround = true;
			player::glidingEnergy = 0;
			player::glidingSpeed = 0;
			player::glidingNow = false;
			if (ydam - (ypos + ya) > 0) {
				player::health -= (ydam - (ypos + ya)) * player::dropDamagePerBlock;
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
		if (FLY)ya *= 0.8;
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

bool player::putBlock(int x, int y, int z, block blockname) {
	Hitbox::AABB blockbox;
	bool success = false;
	blockbox.xmin = x - 0.5;
	blockbox.xmax = x + 0.5;
	blockbox.ymin = y - 0.5;
	blockbox.ymax = y + 0.5;
	blockbox.zmin = z - 0.5;
	blockbox.zmax = z + 0.5;
	if (((Hitbox::Hit(playerbox, blockbox) == false) || CROSS || BlockInfo(blockname).isSolid() == false) && BlockInfo(world::getblock(x, y, z)).isSolid() == false) {
		world::putblock(x, y, z, blockname);
		success = true;
	}
	return success;
}

bool player::save(string worldn) {
	uint32 curversion = VERSION;
	std::stringstream ss;
	ss << "Worlds\\" << worldn << "\\player.NEWorldPlayer";
	std::ofstream isave(ss.str().c_str(), std::ios::binary | std::ios::out);
	if (!isave.is_open()) return false;
	isave << curversion << OnGround << Running << AirJumps << lookupdown << heading << xpos << ypos << zpos
		<< jump << xlookspeed << ylookspeed << FLY << CROSS << canGliding;
	isave.write((char*)inventory, sizeof(inventory));
	isave.write((char*)inventoryAmount, sizeof(inventoryAmount));
	isave << indexInHand;
	isave.close();
	return true;
}

bool player::load(string worldn) {
	uint32 targetVersion;
	std::stringstream ss;
	ss << "Worlds\\" << worldn << "\\player.NEWorldPlayer";
	std::ifstream iload(ss.str().c_str(), std::ios::binary | std::ios::in);
	if (!iload.is_open()) return false;
	iload >> targetVersion;
	if (targetVersion != VERSION) return false;
	iload >> OnGround >> Running >> AirJumps >> lookupdown >> heading
		>> xpos >> ypos >> zpos >> jump >> xlookspeed >> ylookspeed >> FLY >> CROSS >> canGliding;
	iload.read((char*)inventory, sizeof(inventory));
	iload.read((char*)inventoryAmount, sizeof(inventoryAmount));
	iload >> indexInHand;
	iload.close();
	return true;
}

void player::addItem(item itemname, int amount) {
	//�򱳰��������Ʒ
	const int InvMaxStack = 255;
	for (int i = 3; i >= 0; i--) {
		for (int j = 0; j != 10; j++) {
			if (inventory[i][j] == itemname && inventoryAmount[i][j] < InvMaxStack) {
				//�ҵ�һ��ͬ�����
				if (amount + inventoryAmount[i][j] <= InvMaxStack) {
					inventoryAmount[i][j] += amount;
					break;
				}
				else {
					amount -= InvMaxStack - inventoryAmount[i][j];
					inventoryAmount[i][j] = InvMaxStack;
				}
			}
			else if (inventory[i][j] == blocks::AIR) {
				//�ҵ�һ���հ׸���
				inventory[i][j] = itemname;
				if (amount <= InvMaxStack) {
					inventoryAmount[i][j] = amount;
					break;
				}
				else {
					inventoryAmount[i][j] = InvMaxStack;
					amount -= InvMaxStack;
				}
			}
		}
	}
}

PlayerPacket player::convertToPlayerPacket()
{
	PlayerPacket p;
	p.x = xpos;
	p.y = ypos + height + heightExt;
	p.z = zpos;
	p.heading = heading;
	p.lookupdown = lookupdown;
	p.onlineID = onlineID;
	p.skinID = 0;
	strcpy_s(p.name, name.c_str());
	return p;
}

