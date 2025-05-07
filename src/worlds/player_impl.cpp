module;

#include <spdlog/spdlog.h>

module worlds:player_impl;
import std;
import :worlds;
import :player;

using namespace player;

void Player::update(worlds::World& world) {
    auto player_aabb = aabb();

    // Velocity
    if (_flying || _cross_wall) {
        _velocity *= 0.8;
    } else if (_in_water) {
        _velocity *= 0.6;
        _velocity.y() -= 0.03;
    } else {
        _velocity.x() *= 0.6;
        _velocity.z() *= 0.6;
        _velocity.y() -= 0.03;
    }
    auto velocity_original = _velocity;
    if (!_cross_wall) {
        auto boxes = world.hitboxes(player_aabb.extend(_velocity));
        _velocity = player_aabb.clip_displacement(boxes, _velocity, 1e-8);
    }

    // Position
    _coord += _velocity;

    // State flags
    auto vertical_hit = _velocity.y() != velocity_original.y();
    _near_wall = (velocity_original.x() != _velocity.x() || velocity_original.z() != _velocity.z());

    if (vertical_hit && velocity_original.y() < 0.0) {
        _grounded = true;
        if (velocity_original.y() < -0.4 && _game_mode == GameMode::SURVIVAL) {
            _health += velocity_original.y() * _fall_damage;
        }
    } else {
        _grounded = false;
    }

    _in_water = world.in_water(player_aabb);

    if (_flying || _cross_wall || _grounded) {
        _jumps = 0;
    }

    /*
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

    // Health
    if (_game_mode == GameMode::SURVIVAL) {
        if (_health > 0) {
            if (_coord.y() < -100)
                _health -= (-100 - _coord.y()) / 100;
            if (_health < _max_health)
                _health += _heal_speed;
            if (_health > _max_health)
                _health = _max_health;
        } else {
            spawn();
        }
    }
}

auto Player::put_block(worlds::World& world, Vec3i coord, blocks::Id id) -> bool {
    auto player_aabb = aabb();
    auto block_aabb = AABB3d(Vec3d(coord), Vec3d(coord + 1));
    if ((!player_aabb.intersects(block_aabb) || !block_info(id).solid || _cross_wall)
        && !block_info(world.block_or_air(coord).id).solid) {
        world.put_block(coord, id);
        return true;
    }
    return false;
}
