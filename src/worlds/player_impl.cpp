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

    Vec3 velocityOriginal = velocity;
    if (!crossWall) {
        auto hitboxes = world.hitboxes(Hitbox::Expand(playerbox, velocity.x(), velocity.y(), velocity.z()));
        for (auto const& box: hitboxes) {
            velocity.x() = Hitbox::MaxMoveOnXclip(playerbox, box, velocity.x());
        }
        Hitbox::Move(playerbox, velocity.x(), 0.0, 0.0);
        for (auto const& box: hitboxes) {
            velocity.y() = Hitbox::MaxMoveOnYclip(playerbox, box, velocity.y());
        }
        Hitbox::Move(playerbox, 0.0, velocity.y(), 0.0);
        for (auto const& box: hitboxes) {
            velocity.z() = Hitbox::MaxMoveOnZclip(playerbox, box, velocity.z());
        }
        Hitbox::Move(playerbox, 0.0, 0.0, velocity.z());
    }

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

    positionOld = position;
    position += velocity;

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
    auto blockbox = Hitbox::AABB{
        .xmin = coord.x() - 0.5,
        .ymin = coord.y() - 0.5,
        .zmin = coord.z() - 0.5,
        .xmax = coord.x() + 0.5,
        .ymax = coord.y() + 0.5,
        .zmax = coord.z() + 0.5,
    };

    if ((!Hitbox::Hit(playerbox, blockbox) || crossWall || !block_info(blockname).solid)
        && !block_info(world.block_or_air(coord).id).solid) {
        world.put_block(coord, blockname);
        return true;
    }

    return false;
}

Vec3i Player::chunkCoord() const {
    return worlds::chunk_coord(Vec3i(position));
}
