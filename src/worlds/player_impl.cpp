module;

#include <spdlog/spdlog.h>

module worlds:player_impl;
import std;
import :worlds;
import :player;

using namespace player;

void Player::updatePosition(worlds::World& world, double timeDelta) {
    auto playerbox = getHitbox();

    inWater = world.in_water(playerbox);

    if (!inWater) {
        if (!flying && !crossWall) {
            if (onGround) {
                jump = 0.0;
                airJumps = 0;
                velocity.y() = -0.001;
            } else {
                jump -= 0.03;
                velocity.y() = jump + 0.03 / 2.0;
            }
        } else {
            jump = 0.0;
            airJumps = 0;
        }
    } else {
        jump = 0.0;
        airJumps = MaxAirJumps;
        if (velocity.y() <= 0.001 && !flying && !crossWall) {
            velocity.y() = -0.001;
            if (!onGround)
                velocity.y() -= 0.1;
        }
    }

    if (!flying && !crossWall && inWater) {
        velocity *= 0.6;
    }

    auto velocityOriginal = velocity;
    if (!crossWall)
        velocity = playerbox.clip_displacement(world.hitboxes(playerbox.extend(velocity)), velocity);

    positionOld = position;
    position += velocity;

    // If player was falling
    if (velocity.y() != velocityOriginal.y() && velocityOriginal.y() < 0.0) {
        onGround = true;
        if (velocityOriginal.y() < -0.4 && gamemode == GameMode::Survival) {
            // fall damage
            health += velocityOriginal.y() * dropDamage;
        }
    } else {
        onGround = false;
    }

    // Hit roof
    if (velocity.y() != velocityOriginal.y() && velocityOriginal.y() > 0.0) {
        jump = 0.0;
    }

    /*
    // crouch
    if (onGround) {
        if (jump < -0.005) {
            if (jump <= -(jump - 0.5f))
                heightExt = -(height - 0.5f);
            else
                heightExt = static_cast<float>(jump);
        } else {
            if (heightExt <= -0.005) {
                heightExt *= std::pow(0.8, timeDelta * 30.0);
            }
        }
    }
    */

    nearWall = (velocityOriginal.x() != velocity.x() || velocityOriginal.z() != velocity.z());

    velocity.x() *= 0.8;
    velocity.z() *= 0.8;
    if (flying || crossWall) {
        velocity.y() *= 0.8;
    }
    if (onGround) {
        velocity.x() *= 0.7;
        velocity.y() = 0.0;
        velocity.z() *= 0.7;
    }
}

bool Player::putBlock(worlds::World& world, Vec3i coord, blocks::Id blockname) {
    auto playerbox = getHitbox();
    auto blockbox = AABB3d(Vec3d(coord) - 0.5, Vec3d(coord) + 0.5);
    if ((!playerbox.intersects(blockbox) || crossWall || !block_info(blockname).solid)
        && !block_info(world.block_or_air(coord).id).solid) {
        world.put_block(coord, blockname);
        return true;
    }
    return false;
}

Vec3i Player::chunkCoord() const {
    return worlds::chunk_coord(Vec3i(position));
}
