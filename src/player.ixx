module;

#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

export module player;
import chunks;
import worlds;
import types;
import blocks;
import hitboxes;
import globals;
import vec3;

export namespace Player {
enum GameMode { Survival, Creative };

int gamemode = GameMode::Survival;
bool Flying;
bool CrossWall;

float height = 1.2f;
float heightExt = 0.0f;
bool OnGround = false;
bool Running = false;
bool NearWall = false;
bool inWater = false;
BlockData::Id BlockInHand = Blocks().air;
uint8_t indexInHand = 0;

Hitbox::AABB playerbox;
std::vector<Hitbox::AABB> Hitboxes;

double xa, ya, za, xd, yd, zd;
double health = 20, healthMax = 20, healSpeed = 0.01, dropDamage = 5.0;

double speed;
int AirJumps;
Vec3i ccoord;
double lookupdown, heading, xpos, ypos, zpos;
double xposold, yposold, zposold, jump;
double xlookspeed, ylookspeed;
int intxpos, intypos, intzpos;
int intxposold, intyposold, intzposold;

std::array<std::array<BlockData::Id, 10>, 4> inventory;
std::array<std::array<short, 10>, 4> inventoryAmount;

void InitHitbox(Hitbox::AABB& playerbox) {
    playerbox.xmin = -0.3;
    playerbox.xmax = 0.3;
    playerbox.ymin = -0.85;
    playerbox.ymax = 0.85;
    playerbox.zmin = -0.3;
    playerbox.zmax = 0.3;
}

void InitPosition() {
    xposold = xpos;
    yposold = ypos;
    zposold = zpos;
    ccoord = getChunkPos(Vec3i(static_cast<int>(xpos), static_cast<int>(ypos), static_cast<int>(zpos)));
}

void MoveHitbox(double x, double y, double z) {
    Hitbox::MoveTo(playerbox, x, y + 0.5, z);
}

void updateHitbox() {
    MoveHitbox(xpos, ypos, zpos);
}

void init(double x, double y, double z) {
    xpos = x;
    ypos = y;
    zpos = z;
    InitHitbox(playerbox);
    InitPosition();
    updateHitbox();
}

void spawn() {
    xpos = 0.0;
    ypos = 128.0;
    zpos = 0.0;
    jump = 0.0;
    InitHitbox(playerbox);
    InitPosition();
    updateHitbox();
    health = healthMax;
    for (size_t i = 0; i < inventory.size(); i++)
        for (size_t j = 0; j < inventory[i].size(); j++) {
            inventory[i][j] = Blocks().air;
            inventoryAmount[i][j] = 0;
        }
}

void updatePosition(World& world) {
    inWater = world.inWater(playerbox);
    if (!Flying && !CrossWall && inWater) {
        xa *= 0.6;
        ya *= 0.6;
        za *= 0.6;
    }
    double xal = xa, yal = ya, zal = za;

    if (!CrossWall) {
        Hitboxes.clear();
        Hitboxes = world.getHitboxes(Hitbox::Expand(playerbox, xa, ya, za));
        size_t num = Hitboxes.size();
        if (num > 0) {
            for (size_t i = 0; i < num; i++)
                ya = Hitbox::MaxMoveOnYclip(playerbox, Hitboxes[i], ya);
            Hitbox::Move(playerbox, 0.0, ya, 0.0);
            for (size_t i = 0; i < num; i++)
                xa = Hitbox::MaxMoveOnXclip(playerbox, Hitboxes[i], xa);
            Hitbox::Move(playerbox, xa, 0.0, 0.0);
            for (size_t i = 0; i < num; i++)
                za = Hitbox::MaxMoveOnZclip(playerbox, Hitboxes[i], za);
            Hitbox::Move(playerbox, 0.0, 0.0, za);
        }
    }
    if (ya != yal && yal < 0.0) {
        OnGround = true;
        if (yal < -0.4 && gamemode == Survival)
            health += yal * dropDamage;
    } else
        OnGround = false;
    if (ya != yal && yal > 0.0)
        jump = 0.0;
    if (xa != xal || za != zal)
        NearWall = true;
    else
        NearWall = false;

    xa = (double) ((int) (xa * 100000)) / 100000.0;
    ya = (double) ((int) (ya * 100000)) / 100000.0;
    za = (double) ((int) (za * 100000)) / 100000.0;

    xd = xa;
    yd = ya;
    zd = za;
    xpos += xa;
    ypos += ya;
    zpos += za;
    xa *= 0.8;
    za *= 0.8;
    if (Flying || CrossWall)
        ya *= 0.8;
    if (OnGround)
        xa *= 0.7, ya = 0.0, za *= 0.7;
    updateHitbox();

    ccoord = getChunkPos(Vec3i(static_cast<int>(xpos), static_cast<int>(ypos), static_cast<int>(zpos)));
}

bool putBlock(World& world, Vec3i coord, BlockData::Id blockname) {
    Hitbox::AABB blockbox;
    bool success = false;
    blockbox.xmin = coord.x - 0.5;
    blockbox.ymin = coord.y - 0.5;
    blockbox.zmin = coord.z - 0.5;
    blockbox.xmax = coord.x + 0.5;
    blockbox.ymax = coord.y + 0.5;
    blockbox.zmax = coord.z + 0.5;
    if (!chunkOutOfBound(getChunkPos(coord))
        && (((Hitbox::Hit(playerbox, blockbox) == false) || CrossWall || BlockInfo(blockname).solid == false)
            && BlockInfo(world.getBlock(coord).id).solid == false)) {
        world.setBlock(coord, blockname);
        success = true;
    }
    return success;
}

bool save(std::string worldn) {
    uint32_t curversion = GameVersion;
    std::stringstream ss;
    ss << "worlds/" << worldn << "/player.neworldplayer";
    std::ofstream isave(ss.str().c_str(), std::ios::binary | std::ios::out);
    if (!isave.is_open())
        return false;
    isave.write((char*) &curversion, sizeof(curversion));
    isave.write((char*) &xpos, sizeof(xpos));
    isave.write((char*) &ypos, sizeof(ypos));
    isave.write((char*) &zpos, sizeof(zpos));
    isave.write((char*) &lookupdown, sizeof(lookupdown));
    isave.write((char*) &heading, sizeof(heading));
    isave.write((char*) &jump, sizeof(jump));
    isave.write((char*) &OnGround, sizeof(OnGround));
    isave.write((char*) &Running, sizeof(Running));
    isave.write((char*) &AirJumps, sizeof(AirJumps));
    isave.write((char*) &Flying, sizeof(Flying));
    isave.write((char*) &CrossWall, sizeof(CrossWall));
    isave.write((char*) &indexInHand, sizeof(indexInHand));
    isave.write((char*) &health, sizeof(health));
    isave.write((char*) &gamemode, sizeof(gamemode));
    isave.write((char*) &GameTime, sizeof(GameTime));
    isave.write((char*) inventory.data(), sizeof(inventory));
    isave.write((char*) inventoryAmount.data(), sizeof(inventoryAmount));
    isave.close();
    return true;
}

bool load(std::string worldn) {
    uint32_t targetVersion;
    std::stringstream ss;
    ss << "worlds/" << worldn << "/player.neworldplayer";
    std::ifstream iload(ss.str().c_str(), std::ios::binary | std::ios::in);
    if (!iload.is_open())
        return false;
    iload.read((char*) &targetVersion, sizeof(targetVersion));
    if (targetVersion != GameVersion)
        return false;
    iload.read((char*) &xpos, sizeof(xpos));
    iload.read((char*) &ypos, sizeof(ypos));
    iload.read((char*) &zpos, sizeof(zpos));
    iload.read((char*) &lookupdown, sizeof(lookupdown));
    iload.read((char*) &heading, sizeof(heading));
    iload.read((char*) &jump, sizeof(jump));
    iload.read((char*) &OnGround, sizeof(OnGround));
    iload.read((char*) &Running, sizeof(Running));
    iload.read((char*) &AirJumps, sizeof(AirJumps));
    iload.read((char*) &Flying, sizeof(Flying));
    iload.read((char*) &CrossWall, sizeof(CrossWall));
    iload.read((char*) &indexInHand, sizeof(indexInHand));
    iload.read((char*) &health, sizeof(health));
    iload.read((char*) &gamemode, sizeof(gamemode));
    iload.read((char*) &GameTime, sizeof(GameTime));
    iload.read((char*) inventory.data(), sizeof(inventory));
    iload.read((char*) inventoryAmount.data(), sizeof(inventoryAmount));
    iload.close();
    return true;
}

bool addItem(BlockData::Id itemname, short amount) {
    int const InvMaxStack = 255;
    for (int i = 3; i >= 0; i--) {
        for (int j = 0; j != 10; j++) {
            if (inventory[i][j] == itemname && inventoryAmount[i][j] < InvMaxStack) {
                if (amount + inventoryAmount[i][j] <= InvMaxStack) {
                    inventoryAmount[i][j] += amount;
                    return true;
                } else {
                    amount -= InvMaxStack - inventoryAmount[i][j];
                    inventoryAmount[i][j] = InvMaxStack;
                }
            }
        }
    }
    for (int i = 3; i >= 0; i--) {
        for (int j = 0; j != 10; j++) {
            if (inventory[i][j] == Blocks().air) {
                inventory[i][j] = itemname;
                if (amount <= InvMaxStack) {
                    inventoryAmount[i][j] = amount;
                    return true;
                } else {
                    inventoryAmount[i][j] = InvMaxStack;
                    amount -= InvMaxStack;
                }
            }
        }
    }
    return false;
}

void changeGameMode(int _gamemode) {
    gamemode = _gamemode;
    switch (_gamemode) {
        case Survival:
            Flying = false;
            jump = 0.0;
            CrossWall = false;
            break;

        case Creative:
            Flying = true;
            jump = 0.0;
            break;
    }
}
}
