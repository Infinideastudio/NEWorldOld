#include "Player.h"
#include "World.h"
#include "WorldGen.h"

bool canGliding = false; //滑翔
bool FLY;      //飞行
bool CROSS;    //穿墙 ←_← (Superman!)
double glidingMinimumSpeed = pow(1, 2) / 2;

namespace player
{
    Hitbox::AABB playerbox;
    float height = 1.2f;     //玩家的高度
    float heightExt = 0.0f;
    double xa, ya, za, xd, yd, zd;
    unsigned int onlineID;
    string name;
    bool OnGround = false;
    bool Running = false;
    bool NearWall = false;
    bool inWater = false;
    bool glidingNow = false;
    double glidingEnergy, glidingSpeed;
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

    void InitHitbox()
    {
        playerbox = Hitbox::AABB(Vec3d{ -0.3, -0.8, -0.3 }, Vec3d{ 0.3, 0.8, 0.3 });
    }

    void InitPosition()
    {
        xposold = xpos;
        yposold = ypos;
        zposold = zpos;
        cxt = getchunkpos((int)xpos);
        cxtl = cxt;
        cyt = getchunkpos((int)ypos);
        cytl = cyt;
        czt = getchunkpos((int)zpos);
        cztl = czt;
    }

    void MoveHitbox(double x, double y, double z)
    {
        playerbox.MoveTo(x, y + 0.5, z);
    }

    void MoveHitboxToPosition()
    {
        MoveHitbox(xpos, ypos, zpos);
    }

    void Move()
    {

        inWater = world::inWater(playerbox);

        if (!FLY && !CROSS && inWater)
        {
            xa *= 0.6;
            ya *= 0.6;
            za *= 0.6;
        }

        double xal = xa;
        double yal = ya;
        double zal = za;

        if (!CROSS)
        {

            vector<Hitbox::AABB> Hitboxes = world::getHitboxes(playerbox.Expand(xa, ya, za));
            int num = Hitboxes.size();

            if (num > 0)
            {
                for (int i = 0; i < num; i++)
                {
                    xa = Hitbox::maxMoveOnXclip(playerbox, Hitboxes[i], xa);
                }

                playerbox.Move(xa, 0.0, 0.0);

                for (int i = 0; i < num; i++)
                {
                    za = Hitbox::maxMoveOnZclip(playerbox, Hitboxes[i], za);
                }

                playerbox.Move(0.0, 0.0, za);

                for (int i = 0; i < num; i++)
                {
                    ya = Hitbox::maxMoveOnYclip(playerbox, Hitboxes[i], ya);
                }

                playerbox.Move(0.0, ya, 0.0);
            }

            if (ya != yal && yal < 0.0)
            {
                OnGround = true;
                player::glidingEnergy = 0;
                player::glidingSpeed = 0;
                player::glidingNow = false;
            }
            else
            {
                OnGround = false;
            }

            if (ya != yal && yal > 0.0)
            {
                jump = 0.0;
            }

            if (xa != xal || za != zal)
            {
                NearWall = true;
            }
            else
            {
                NearWall = false;
            }

            xd = xa;
            yd = ya;
            zd = za;
            xpos += xa;
            ypos += ya;
            zpos += za;
            xa *= 0.8;
            za *= 0.8;

            if (OnGround)
            {
                xa *= 0.7;
                za *= 0.7;
            }

            if (FLY)
            {
                ya *= 0.8;
            }
        }
        else
        {
            xpos += xa;
            ypos += ya;
            zpos += za;

            xa *= 0.8;
            ya *= 0.8;
            za *= 0.8;
        }

        cxtl = cxt;
        cytl = cyt;
        cztl = czt;
        cxt = getchunkpos((int)xpos);
        cyt = getchunkpos((int)ypos);
        czt = getchunkpos((int)zpos);

        MoveHitboxToPosition();
    }

    bool putblock(int x, int y, int z, block blockname)
    {
        Hitbox::AABB blockbox;
        bool success = false;
        blockbox.min.x = x - 0.5;
        blockbox.max.x = x + 0.5;
        blockbox.min.y = y - 0.5;
        blockbox.max.y = y + 0.5;
        blockbox.min.z = z - 0.5;
        blockbox.max.z = z + 0.5;

        if (((Hitbox::hit(playerbox, blockbox) == false) || CROSS || BlockInfo(blockname).isSolid() == false) && BlockInfo(world::getblock(x, y, z)).isSolid() == false)
        {
            world::setblock(x, y, z, blockname);
            success = true;
        }

        return success;
    }

    bool save(string worldn)
    {
        uint32 curversion = VERSION;
        std::stringstream ss;
        ss << "Worlds\\" << worldn << "\\player.NEWorldPlayer";
        std::ofstream isave(ss.str().c_str(), std::ios::binary | std::ios::out);

        if (!isave.is_open())
        {
            return false;
        }

		isave.write((char*)&WorldGen::seed, sizeof(WorldGen::seed));
        isave.write((char *)&curversion, sizeof(curversion));
        isave.write((char *)&xpos, sizeof(xpos));
        isave.write((char *)&ypos, sizeof(ypos));
        isave.write((char *)&zpos, sizeof(zpos));
        isave.write((char *)&lookupdown, sizeof(lookupdown));
        isave.write((char *)&heading, sizeof(heading));
        isave.write((char *)&jump, sizeof(jump));
        isave.write((char *)&OnGround, sizeof(OnGround));
        isave.write((char *)&Running, sizeof(Running));
        isave.write((char *)&AirJumps, sizeof(AirJumps));
        isave.write((char *)&FLY, sizeof(FLY));
        isave.write((char *)&CROSS, sizeof(CROSS));
        isave.write((char *)&itemInHand, sizeof(itemInHand));
        isave.write((char *)inventorybox, sizeof(inventorybox));
        isave.write((char *)inventorypcs, sizeof(inventorypcs));
        isave.close();
        return true;
    }

    bool load(string worldn)
    {
        uint32 targetVersion;
        std::stringstream ss;
        ss << "Worlds\\" << worldn << "\\player.NEWorldPlayer";
        std::ifstream iload(ss.str().c_str(), std::ios::binary | std::ios::in);

        if (!iload.is_open())
        {
            return false;
        }

        iload.read((char *)&targetVersion, sizeof(targetVersion));

        if (targetVersion != VERSION)
        {
            return false;
        }

		iload.read((char*)&WorldGen::seed, sizeof(WorldGen::seed));
		printf("[xxx]:WorldSeed:%d", WorldGen::seed);
        iload.read((char *)&xpos, sizeof(xpos));
        iload.read((char *)&ypos, sizeof(ypos));
        iload.read((char *)&zpos, sizeof(zpos));
        iload.read((char *)&lookupdown, sizeof(lookupdown));
        iload.read((char *)&heading, sizeof(heading));
        iload.read((char *)&jump, sizeof(jump));
        iload.read((char *)&OnGround, sizeof(OnGround));
        iload.read((char *)&Running, sizeof(Running));
        iload.read((char *)&AirJumps, sizeof(AirJumps));
        iload.read((char *)&FLY, sizeof(FLY));
        iload.read((char *)&CROSS, sizeof(CROSS));
        iload.read((char *)&itemInHand, sizeof(itemInHand));
        iload.read((char *)inventorybox, sizeof(inventorybox));
        iload.read((char *)inventorypcs, sizeof(inventorypcs));
        iload.close();
        return true;
    }

    void additem(block itemname)
    {
        //向背包里加入物品
        bool f = false;

        for (int i = 0; i != 10; i++)
        {
            if (inventorybox[3][i] == blocks::AIR)
            {
                inventorybox[3][i] = itemname;
                inventorypcs[3][i] = 1;
                f = true;
            }
            else if (inventorybox[3][i] == itemname && inventorypcs[3][i] < 255)
            {
                inventorypcs[3][i]++;
                f = true;
            }

            if (f)
            {
                break;
            }
        }

        if (!f)
        {
            for (int i = 0; i != 3; i++)
            {
                for (int j = 0; j != 10; j++)
                {
                    if (inventorybox[i][j] == blocks::AIR)
                    {
                        inventorybox[i][j] = itemname;
                        inventorypcs[i][j] = 1;
                        f = true;
                    }
                    else if (inventorybox[i][j] == itemname && inventorypcs[i][j] < 255)
                    {
                        inventorypcs[i][j]++;
                        f = true;
                    }

                    if (f)
                    {
                        break;
                    }
                }
            }
        }
    }

}
