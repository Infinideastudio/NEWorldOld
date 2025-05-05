module;

#include <spdlog/spdlog.h>

export module worlds:player;

import std;
import types;
import chunks;
import blocks;
import hitboxes;
import globals;
import vec3;
import items;
import :forward;

export namespace player {

class Player {
public:
    enum class GameMode { Survival, Creative };

    Player() = default;
    Player(std::string const& worldName);

    void updatePosition(worlds::World& world, double timeDelta);
    bool putBlock(worlds::World& world, Vec3i coord, blocks::Id blockname);
    bool save(std::string const& worldName) const;
    bool addItem(items::ItemStack stack);
    void clearInventory() {
        inventory = {};
    }
    void changeGameMode(GameMode mode);
    void spawn() {
        setPosition({0, 128, 0});
        health = healthMax;
    }

    // Getters and setters for encapsulated fields
    double getHealth() const {
        return health;
    }
    double getMaxHealth() const {
        return healthMax;
    }
    void setHealth(double value) {
        health = std::clamp(value, 0.0, healthMax);
    }

    GameMode getGameMode() const {
        return gamemode;
    }
    void setGameMode(GameMode mode) {
        changeGameMode(mode);
    }

    void setPosition(Vec3d const position) {
        this->position = position;
        this->positionOld = position;
    }
    Vec3d getPosition() const {
        return position;
    }
    Vec3d getPositionOld() const {
        return positionOld;
    }
    Hitbox::AABB getHitbox() const {
        return {
            .xmin = position.x - 0.3,
            .ymin = position.y - 0.0,
            .zmin = position.z - 0.3,
            .xmax = position.x + 0.3,
            .ymax = position.y + 1.7,
            .zmax = position.z + 0.3,
        };
    }

    void update() {
        // health
        if (health > 0 || gamemode == GameMode::Creative) {
            if (position.y < -100)
                health -= (-100 - position.y) / 100;
            if (health < healthMax)
                health += healSpeed;
            if (health > healthMax)
                health = healthMax;
        } else
            spawn();
    }
    void updatePosition() {}

    Vec3i chunkCoord() const;
    Vec3d lookCoord() const {
        return {position.x, position.y + height, position.z};
    }
    items::ItemStack& holdingItemStack() {
        return inventory[3][indexInHand];
    }

    double getHeading() const {
        return heading;
    }
    double getLookUpDown() const {
        return lookUpDown;
    }
    void setLookUpDown(double value) {
        lookUpDown = std::clamp(value, -90.0, 90.0);
    }
    void setHeading(double value) {
        heading = std::fmod(value, 360.0);
        if (heading < 0)
            heading += 360.0;
    }
    void updateHeadingAndLookUpDown() {
        heading += xlookSpeed;
        lookUpDown += ylookSpeed;
        lookUpDown = std::clamp(lookUpDown, -90.0, 90.0);
        xlookSpeed = ylookSpeed = 0.0;
    }
    void setRunning(bool v) {
        running = v;
        speed = running ? RunSpeed : WalkSpeed;
    }
    bool isRunning() const {
        return running;
    }
    void setVelocity(Vec3d newVelocity) {
        velocity = newVelocity;
    }
    Vec3d getVelocity() const {
        return velocity;
    }
    double getSpeed() const {
        return speed;
    }
    bool isOnGround() const {
        return onGround;
    }
    bool isNearWall() const {
        return nearWall;
    }
    bool isInWater() const {
        return inWater;
    }
    bool isFlying() const {
        return flying;
    }
    bool canCrossWall() const {
        return crossWall;
    }
    void setCanCrossWall(bool v) {
        crossWall = v;
    }
    size_t getIndexInHand() const {
        return indexInHand;
    }
    void setIndexInHand(size_t index) {
        indexInHand = std::clamp(index, static_cast<size_t>(0), static_cast<size_t>(9));
    }
    void setXLookSpeed(double value) {
        xlookSpeed = value;
    }
    void setYLookSpeed(double value) {
        ylookSpeed = value;
    }
    double getXLookSpeed() const {
        return xlookSpeed;
    }
    double getYLookSpeed() const {
        return ylookSpeed;
    }
    items::ItemStack& getInventory(size_t row, size_t col) {
        return inventory[row][col];
    }

    void processJump(bool justPressed) {
        if (!flying && !crossWall) {
            if (!isInWater()) {
                if (justPressed && airJumps < MaxAirJumps || onGround) {
                    if (!onGround) {
                        jump = 0.3;
                        airJumps++;
                    } else {
                        jump = 0.3;
                        onGround = false;
                    }
                }
            } else {
                velocity.y = 0.2;
            }
        } else {
            velocity.y += WalkSpeed / 2;
        }
    }


private:
    static float constexpr WalkSpeed = 0.15f;
    static float constexpr RunSpeed = 0.3f;
    static int constexpr MaxAirJumps = 3 - 1;

    // Player attributes
    GameMode gamemode = GameMode::Survival;
    bool flying = false;
    bool crossWall = false;

    double height = 1.35;
    bool onGround = false;
    bool running = false;
    bool nearWall = false;
    bool inWater = false;

    uint8_t indexInHand = 0;

    double health = 20, healthMax = health, healSpeed = 0.01, dropDamage = 5.0;

    double speed = 0;
    int airJumps = 0;
    double lookUpDown = 0, heading = 0, jump = 0;
    Vec3d position = {0, 128, 0}, positionOld = position, velocity = {0, 0, 0};
    double xlookSpeed = 0, ylookSpeed = 0;

    std::array<std::array<items::ItemStack, 10>, 4> inventory{};
};

bool Player::save(std::string const& worldName) const {
    uint32_t curVersion = GameVersion;
    std::stringstream ss;
    ss << "worlds/" << worldName << "/player.neworldplayer";
    std::ofstream saveFile(ss.str(), std::ios::binary | std::ios::out);
    if (!saveFile.is_open()) {
        return false;
    }

    saveFile.write(reinterpret_cast<char const*>(&curVersion), sizeof(curVersion));
    saveFile.write(reinterpret_cast<char const*>(&position.x), sizeof(position.x));
    saveFile.write(reinterpret_cast<char const*>(&position.y), sizeof(position.y));
    saveFile.write(reinterpret_cast<char const*>(&position.z), sizeof(position.z));
    saveFile.write(reinterpret_cast<char const*>(&lookUpDown), sizeof(lookUpDown));
    saveFile.write(reinterpret_cast<char const*>(&heading), sizeof(heading));
    saveFile.write(reinterpret_cast<char const*>(&jump), sizeof(jump));
    saveFile.write(reinterpret_cast<char const*>(&onGround), sizeof(onGround));
    saveFile.write(reinterpret_cast<char const*>(&running), sizeof(running));
    saveFile.write(reinterpret_cast<char const*>(&airJumps), sizeof(airJumps));
    saveFile.write(reinterpret_cast<char const*>(&flying), sizeof(flying));
    saveFile.write(reinterpret_cast<char const*>(&crossWall), sizeof(crossWall));
    saveFile.write(reinterpret_cast<char const*>(&indexInHand), sizeof(indexInHand));
    saveFile.write(reinterpret_cast<char const*>(&health), sizeof(health));
    saveFile.write(reinterpret_cast<char const*>(&gamemode), sizeof(gamemode));
    saveFile.write(reinterpret_cast<char const*>(&GameTime), sizeof(GameTime));
    saveFile.write(reinterpret_cast<char const*>(inventory.data()), sizeof(inventory));
    saveFile.close();
    return true;
}

Player::Player(std::string const& worldName) {
    spdlog::debug("Loading player data");
    uint32_t targetVersion;
    std::stringstream ss;
    ss << "worlds/" << worldName << "/player.neworldplayer";
    std::ifstream loadFile(ss.str(), std::ios::binary | std::ios::in);
    if (!loadFile.is_open()) {
        spdlog::error("Failed to open player data file");
        throw std::runtime_error("Failed to open player data file");
    }

    loadFile.read(reinterpret_cast<char*>(&targetVersion), sizeof(targetVersion));
    if (targetVersion != GameVersion) {
        spdlog::error("Player data version mismatch: expected {}, got {}", GameVersion, targetVersion);
        throw std::runtime_error("Player data version mismatch");
    }

    loadFile.read(reinterpret_cast<char*>(&position.x), sizeof(position.x));
    loadFile.read(reinterpret_cast<char*>(&position.y), sizeof(position.y));
    loadFile.read(reinterpret_cast<char*>(&position.z), sizeof(position.z));
    loadFile.read(reinterpret_cast<char*>(&lookUpDown), sizeof(lookUpDown));
    loadFile.read(reinterpret_cast<char*>(&heading), sizeof(heading));
    loadFile.read(reinterpret_cast<char*>(&jump), sizeof(jump));
    loadFile.read(reinterpret_cast<char*>(&onGround), sizeof(onGround));
    loadFile.read(reinterpret_cast<char*>(&running), sizeof(running));
    loadFile.read(reinterpret_cast<char*>(&airJumps), sizeof(airJumps));
    loadFile.read(reinterpret_cast<char*>(&flying), sizeof(flying));
    loadFile.read(reinterpret_cast<char*>(&crossWall), sizeof(crossWall));
    loadFile.read(reinterpret_cast<char*>(&indexInHand), sizeof(indexInHand));
    loadFile.read(reinterpret_cast<char*>(&health), sizeof(health));
    loadFile.read(reinterpret_cast<char*>(&gamemode), sizeof(gamemode));
    loadFile.read(reinterpret_cast<char*>(&GameTime), sizeof(GameTime));
    loadFile.read(reinterpret_cast<char*>(inventory.data()), sizeof(inventory));
    loadFile.close();

    positionOld = position;
}

bool Player::addItem(items::ItemStack stack) {
    constexpr int InvMaxStack = 255;
    for (int i = 3; i >= 0; i--) {
        auto& row = inventory[i];
        for (size_t j = 0; j < row.size(); ++j) {
            if (row[j].id == stack.id && row[j].count < InvMaxStack) {
                if (stack.count + row[j].count <= InvMaxStack) {
                    row[j].count += stack.count;
                    return true;
                }
                stack.count -= InvMaxStack - row[j].count;
                row[j].count = InvMaxStack;
            }
        }
    }

    for (int i = 3; i >= 0; i--) {
        auto& row = inventory[i];
        for (size_t j = 0; j < row.size(); ++j) {
            if (row[j].empty()) {
                row[j].id = stack.id;
                if (stack.count <= InvMaxStack) {
                    row[j].count = stack.count;
                    return true;
                }

                row[j].count = InvMaxStack;
                stack.count -= InvMaxStack;
            }
        }
    }
    return false;
}

void Player::changeGameMode(GameMode mode) {
    gamemode = mode;
    switch (mode) {
        case GameMode::Survival:
            flying = false;
            jump = 0.0;
            crossWall = false;
            break;

        case GameMode::Creative:
            flying = true;
            jump = 0.0;
            break;
    }
}

} // namespace Player