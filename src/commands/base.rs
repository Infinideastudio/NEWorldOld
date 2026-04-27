//! `register_base_commands` — registers the 12 base-game slash commands.
//!
//! Mirrors C++ `commands.ixx::register_base_commands`. `_base` is currently
//! unused but kept on the signature for future commands that need named-block
//! lookups; `block_registry` is `Arc`'d so the closures can capture it cheaply
//! and stay `Send + Sync`.

use std::str::FromStr;
use std::sync::Arc;

use std::collections::{HashSet, VecDeque};

use crate::blocks::{self, BaseBlocks, BlockRegistry};
use crate::items::ItemStack;
use crate::math::{Vec3d, Vec3i};
use crate::worlds::player::GameMode;

use super::{Command, CommandRegistry};

/// Mirrors the C++ `_parse_int<T>(string_view)` helper. Returns `None` if the
/// string does not parse cleanly as `T`.
fn parse_int<T: FromStr>(s: &str) -> Option<T> {
    s.parse::<T>().ok()
}

/// Mirrors the C++ `_parse_float<T>(string_view)` helper.
fn parse_float<T: FromStr>(s: &str) -> Option<T> {
    s.parse::<T>().ok()
}

/// Register the same set of slash-commands the C++ `register_base_commands`
/// does, plus two LCM3 helpers (`/lcm3-clock-rate`, `/lcm3-reset`) that
/// the Rust port adds for the in-progress LCM3 circuit subsystem.
#[allow(clippy::needless_pass_by_value)] // Arc moved into the closures by clone.
pub fn register_base_commands(
    registry: &mut CommandRegistry,
    _base: &BaseBlocks,
    block_registry: Arc<BlockRegistry>,
) {
    // /help
    registry.add(
        "/help",
        Command::new(|args, _world, messages| {
            if args.len() != 1 {
                return false;
            }
            messages.push(
                "Controls: W/A/S/D/SPACE/SHIFT = move, R/F = fast move (creative mode), E = open inventory,"
                    .to_owned(),
            );
            messages.push(
                "          left/right mouse button = break/place blocks, mouse wheel = select blocks,"
                    .to_owned(),
            );
            messages.push(
                "          F1 = switch game mode, F2 = take screenshot, F3 = switch debug panel,"
                    .to_owned(),
            );
            messages.push(
                "          F4 = switch cross wall (creative mode), F5 = switch HUD,".to_owned(),
            );
            messages.push(
                "          F7 = switch full screen mode, F8 = fast forward game time".to_owned(),
            );
            messages.push(
                "Commands: /help | /clear | /kit | /give <id> <amount> | /tp <x> <y> <z> | /clearinventory |"
                    .to_owned(),
            );
            messages.push(
                "          /setblock <x> <y> <z> <id> | /tree <x> <y> <z> | /explode <x> <y> <z> <radius> | /time <time>"
                    .to_owned(),
            );
            true
        }),
    );

    // /clear
    registry.add(
        "/clear",
        Command::new(|args, _world, messages| {
            if args.len() != 1 {
                return false;
            }
            messages.clear();
            true
        }),
    );

    // /kit
    {
        let block_registry = Arc::clone(&block_registry);
        registry.add(
            "/kit",
            Command::new(move |args, world, _messages| {
                if args.len() != 1 {
                    return false;
                }
                let player = world.player_mut();
                for i in 0..block_registry.entries().len() {
                    let id = blocks::Id(u16::try_from(i).unwrap_or(u16::MAX));
                    player.add_item(ItemStack::new(id, ItemStack::MAX_COUNT));
                }
                true
            }),
        );
    }

    // /give <id> <amount>
    registry.add(
        "/give",
        Command::new(|args, world, _messages| {
            if args.len() != 3 {
                return false;
            }
            let Some(id) = parse_int::<u16>(args[1]) else {
                return false;
            };
            let Some(amount) = parse_int::<u32>(args[2]) else {
                return false;
            };
            // ItemStack::MAX_COUNT == 255; saturate.
            let amount = u8::try_from(amount.min(u32::from(ItemStack::MAX_COUNT)))
                .unwrap_or(ItemStack::MAX_COUNT);
            world
                .player_mut()
                .add_item(ItemStack::new(blocks::Id(id), amount));
            true
        }),
    );

    // /tp <x> <y> <z>
    registry.add(
        "/tp",
        Command::new(|args, world, _messages| {
            if args.len() != 4 {
                return false;
            }
            let Some(x) = parse_float::<f64>(args[1]) else {
                return false;
            };
            let Some(y) = parse_float::<f64>(args[2]) else {
                return false;
            };
            let Some(z) = parse_float::<f64>(args[3]) else {
                return false;
            };
            world.player_mut().set_coord(Vec3d::new(x, y, z));
            true
        }),
    );

    // /clearinventory
    registry.add(
        "/clearinventory",
        Command::new(|args, world, _messages| {
            if args.len() != 1 {
                return false;
            }
            world.player_mut().clear_inventory();
            true
        }),
    );

    // /setblock <x> <y> <z> <id>
    registry.add(
        "/setblock",
        Command::new(|args, world, _messages| {
            if args.len() != 5 {
                return false;
            }
            let Some(x) = parse_int::<i32>(args[1]) else {
                return false;
            };
            let Some(y) = parse_int::<i32>(args[2]) else {
                return false;
            };
            let Some(z) = parse_int::<i32>(args[3]) else {
                return false;
            };
            let Some(id) = parse_int::<u16>(args[4]) else {
                return false;
            };
            world.set_block(Vec3i::new(x, y, z), blocks::Id(id), true);
            true
        }),
    );

    // /tree <x> <y> <z>
    registry.add(
        "/tree",
        Command::new(|args, world, _messages| {
            if args.len() != 4 {
                return false;
            }
            let Some(x) = parse_int::<i32>(args[1]) else {
                return false;
            };
            let Some(y) = parse_int::<i32>(args[2]) else {
                return false;
            };
            let Some(z) = parse_int::<i32>(args[3]) else {
                return false;
            };
            world.build_tree(Vec3i::new(x, y, z));
            true
        }),
    );

    // /explode <x> <y> <z> <radius>
    registry.add(
        "/explode",
        Command::new(|args, world, _messages| {
            if args.len() != 5 {
                return false;
            }
            let Some(x) = parse_int::<i32>(args[1]) else {
                return false;
            };
            let Some(y) = parse_int::<i32>(args[2]) else {
                return false;
            };
            let Some(z) = parse_int::<i32>(args[3]) else {
                return false;
            };
            let Some(r) = parse_int::<i32>(args[4]) else {
                return false;
            };
            world.explode(Vec3i::new(x, y, z), r);
            true
        }),
    );

    // /time <time>
    //
    // Unlike the C++ version (a no-op TODO because game time was a global),
    // `World` already owns `game_time`, so the Rust port wires this up.
    registry.add(
        "/time",
        Command::new(|args, world, _messages| {
            if args.len() != 2 {
                return false;
            }
            let Some(t) = parse_int::<u32>(args[1]) else {
                // C++ also rejects negative values; `u32::from_str` refuses
                // leading `-`, so this branch covers both.
                return false;
            };
            world.set_game_time(t);
            true
        }),
    );

    // /gamemode <mode>
    registry.add(
        "/gamemode",
        Command::new(|args, world, _messages| {
            if args.len() != 2 {
                return false;
            }
            let Some(raw) = parse_int::<u32>(args[1]) else {
                return false;
            };
            let mode = match raw {
                0 => GameMode::Survival,
                1 => GameMode::Creative,
                _ => return false,
            };
            world.player_mut().set_game_mode(mode);
            true
        }),
    );

    // /lcm3-clock-rate <number>
    registry.add(
        "/lcm3-clock-rate",
        Command::new(|args, world, messages| {
            if args.len() != 2 {
                return false;
            }
            let Some(rate) = parse_int::<usize>(args[1]) else {
                return false;
            };
            world.set_lcm3_clock_rate(rate);
            messages.push(format!("LCM3 clock rate set to {rate}"));
            true
        }),
    );

    // /lcm3-reset <x> <y> <z>
    //
    // BFS the connected component of LCM3 blocks containing `(x, y, z)`
    // (face-sharing only; non-LCM3 cells terminate propagation) and
    // reset each cell's interior — data + clock → 0 — while preserving
    // its placement orientation. Mirrors the LCM3 design in
    // `docs/block_updates.md`: the reset gives the whole connected
    // sub-circuit a clean slate without disturbing the wiring.
    let registry_for_reset = Arc::clone(&block_registry);
    registry.add(
        "/lcm3-reset",
        Command::new(move |args, world, messages| {
            if args.len() != 4 {
                return false;
            }
            let Some(x) = parse_int::<i32>(args[1]) else {
                return false;
            };
            let Some(y) = parse_int::<i32>(args[2]) else {
                return false;
            };
            let Some(z) = parse_int::<i32>(args[3]) else {
                return false;
            };
            let start = Vec3i::new(x, y, z);
            let base = world.base_blocks();
            // Reject up front if the seed cell isn't a loaded LCM3
            // block — avoids confusing "reset 0 blocks" output and
            // surfaces typos.
            let Some(seed) = world.block(start) else {
                messages.push(format!(
                    "/lcm3-reset: chunk at ({x}, {y}, {z}) is not loaded"
                ));
                return true;
            };
            if !base.is_lcm3(seed.id) {
                messages.push(format!(
                    "/lcm3-reset: block at ({x}, {y}, {z}) is not an LCM3 block"
                ));
                return true;
            }

            const NEIGHBOURS: [Vec3i; 6] = [
                Vec3i::new(1, 0, 0),
                Vec3i::new(-1, 0, 0),
                Vec3i::new(0, 1, 0),
                Vec3i::new(0, -1, 0),
                Vec3i::new(0, 0, 1),
                Vec3i::new(0, 0, -1),
            ];
            let mut visited: HashSet<Vec3i> = HashSet::new();
            let mut queue: VecDeque<Vec3i> = VecDeque::new();
            visited.insert(start);
            queue.push_back(start);
            let mut reset_count: usize = 0;
            while let Some(coord) = queue.pop_front() {
                let Some(cell) = world.block(coord) else {
                    continue; // unloaded: stop propagation here.
                };
                if !base.is_lcm3(cell.id) {
                    continue; // non-LCM3: stop propagation here.
                }
                let info = registry_for_reset.get(cell.id);
                let new_state = info.orientation_codec.reset_to_base(cell.state);
                if new_state != cell.state {
                    world.set_block_with_state(coord, cell.id, new_state, true);
                }
                reset_count += 1;
                for d in NEIGHBOURS {
                    let nb = coord + d;
                    if visited.insert(nb) {
                        queue.push_back(nb);
                    }
                }
            }
            messages.push(format!(
                "/lcm3-reset: reset {reset_count} LCM3 block{} starting at ({x}, {y}, {z})",
                if reset_count == 1 { "" } else { "s" }
            ));
            true
        }),
    );
}
