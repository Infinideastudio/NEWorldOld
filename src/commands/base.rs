//! `register_base_commands` — registers the 12 base-game slash commands.
//!
//! Mirrors C++ `commands.ixx::register_base_commands`. `_base` is currently
//! unused but kept on the signature for future commands that need named-block
//! lookups; `block_registry` is `Arc`'d so the closures can capture it cheaply
//! and stay `Send + Sync`.

use std::str::FromStr;
use std::sync::Arc;

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
/// does (12 entries, same arity and argument shape).
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
                "Commands: /help | /clear | /kit | /give <id> <amount> | /tp <x> <y> <z> | /clearinventory | /suicide |"
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

    // /suicide
    registry.add(
        "/suicide",
        Command::new(|args, world, _messages| {
            if args.len() != 1 {
                return false;
            }
            world.player_mut().spawn();
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
}
