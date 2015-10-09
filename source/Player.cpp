#include "Player.h"
#include "World.h"

bool FLY;      //飞行
bool CROSS;    //穿墙 ←_← (Superman!)
namespace player{
	Hitbox::AABB playerbox;
	float height = 1.2f;     //玩家的高度
	float heightExt = 0.0f;
	double xa, ya, za, xd, yd, zd;
	bool OnGround = false;
	bool Running = false;
	bool NearWall = false;
	bool inWater = false;
	double speed;
	int AirJumps;
	int cxt, cyt, czt, cxtl, cytl, cztl;
	double lookupdown, heading, xpos, ypos, zpos, xposold, yposold, zposold, jump;
	double xlookspeed, ylookspeed;
	int intxpos, intypos, intzpos, intxposold, intyposold, intzposold;
	block BlockInHand = blocks::AIR;
	ubyte itemInHand = 0;
	block inventorybox[4][10];
	block inventorypcs[4][10];

	void InitHitbox(){
		playerbox.xmin = -0.3;
		playerbox.xmax = 0.3;
		playerbox.ymin = -0.8;
		playerbox.ymax = 0.8;
		playerbox.zmin = -0.3;
		playerbox.zmax = 0.3;
	}

	void InitPosition(){
		xposold = xpos;
		yposold = ypos;
		zposold = zpos;
		cxt = getchunkpos((int)xpos); cxtl = cxt;
		cyt = getchunkpos((int)ypos); cytl = cyt;
		czt = getchunkpos((int)zpos); cztl = czt;
	}

	void MoveHitbox(double x, double y, double z){
		Hitbox::MoveTo(playerbox, x, y + 0.5, z);
	}

	void MoveHitboxToPosition(){
		MoveHitbox(xpos, ypos, zpos);
	}

	void Move(){

		inWater = world::inWater(playerbox);
		if (!FLY && !CROSS && inWater){
			xa *= 0.6;
			ya *= 0.6;
			za *= 0.6;
		}

		double xal = xa;
		double yal = ya;
		double zal = za;

		if (!CROSS){
			
			vector<Hitbox::AABB> Hitboxes = world::getHitboxes(Hitbox::Expand(playerbox, xa, ya, za));
			int num = Hitboxes.size();
			if (num > 0){
				for (int i = 0; i < num; i++){
					ya = Hitbox::MaxMoveOnYclip(playerbox, Hitboxes[i], ya);
				}
				Hitbox::Move(playerbox, 0.0, ya, 0.0);
				for (int i = 0; i < num; i++){
					xa = Hitbox::MaxMoveOnXclip(playerbox, Hitboxes[i], xa);
				}
				Hitbox::Move(playerbox, xa, 0.0, 0.0);
				for (int i = 0; i < num; i++){
					za = Hitbox::MaxMoveOnZclip(playerbox, Hitboxes[i], za);
				}
				Hitbox::Move(playerbox, 0.0, 0.0, za);
			}
			if (ya != yal && yal<0.0) OnGround = true; else OnGround = false;
			if (ya != yal && yal>0.0) jump = 0.0;
			if (xa != xal || za != zal) NearWall = true; else NearWall = false;
			xd = xa; yd = ya; zd = za;
			xpos += xa; ypos += ya; zpos += za;
			xa *= 0.8;
			za *= 0.8;
			if (OnGround){
				xa *= 0.7;
				za *= 0.7;
			}
			if (FLY)ya *= 0.8;
		}
		else{
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

		MoveHitboxToPosition();
	}

	bool putblock(int x, int y, int z, block blockname) {
		Hitbox::AABB blockbox;
		bool success = false;
		blockbox.xmin = x - 0.5;
		blockbox.xmax = x + 0.5;
		blockbox.ymin = y - 0.5;
		blockbox.ymax = y + 0.5;
		blockbox.zmin = z - 0.5;
		blockbox.zmax = z + 0.5;
		if (((Hitbox::Hit(playerbox, blockbox) == false) || CROSS || BlockInfo(blockname).isSolid() == false) && BlockInfo(world::getblock(x, y, z)).isSolid() == false){
			world::putblock(x, y, z, blockname);
			success = true;
		}
		return success;
	}

	void save(string worldn){
		uint32 curversion = VERSION;

		std::stringstream ss;
		ss << "Worlds\\" << worldn << "\\player.NEWorldPlayer";
		std::ofstream isave(ss.str().c_str(), std::ios::binary | std::ios::out);
		if (!isave.is_open()) return;
		isave << curversion << OnGround << Running << AirJumps << lookupdown << heading << xpos << ypos << zpos
			  << jump << xlookspeed << ylookspeed << FLY << CROSS;
		isave.write((char*)inventorybox, sizeof(inventorybox));
		isave.write((char*)inventorypcs, sizeof(inventorypcs));
		isave << itemInHand;
		isave.close();
	}

	void load(string worldn){
		uint32 targetVersion;
		std::stringstream ss;
		ss <<"Worlds\\" << worldn << "player.NEWorldPlayer";
		std::ifstream iload(ss.str().c_str(), std::ios::binary | std::ios::in);
		if (!iload.is_open()) return;
		iload >> targetVersion;
		if (targetVersion != VERSION) return;
		iload >> OnGround >> Running >> AirJumps >> lookupdown >> heading
			  >> xpos >> ypos >> zpos >> jump >> xlookspeed >> ylookspeed >> FLY >> CROSS;
		iload.read((char*)inventorybox, sizeof(inventorybox));
		iload.read((char*)inventorypcs, sizeof(inventorypcs));
		iload.close();
	}

	void additem(block itemname){
		//向背包里加入物品
		bool f = false;
		for (int i = 0; i != 10; i++){
			if (inventorybox[3][i] == blocks::AIR){
				inventorybox[3][i] = itemname;
				inventorypcs[3][i] = 1;
				f = true;
			}
			else if (inventorybox[3][i] == itemname && inventorypcs[3][i] < 255){
				inventorypcs[3][i]++;
				f = true;
			}
			if (f) break;
		}
		if (!f){
			for (int i = 0; i != 3; i++){
				for (int j = 0; j != 10; j++){
					if (inventorybox[i][j] == blocks::AIR){
						inventorybox[i][j] = itemname;
						inventorypcs[i][j] = 1;
						f = true;
					}
					else if (inventorybox[i][j] == itemname && inventorypcs[i][j] < 255){
						inventorypcs[i][j]++;
						f = true;
					}

					if (f) break;
				}
			}
		}
	}

}
