//! `register_base_commands` — registers the 12 base-game slash commands.
//!
//! Mirrors C++ `commands.ixx::register_base_commands`. `_base` is currently
//! unused but kept on the signature for future commands that need named-block
//! lookups; `block_registry` is `Arc`'d so the closures can capture it cheaply
//! and stay `Send + Sync`.

use std::str::FromStr;
use std::sync::Arc;

use crate::blocks::{self, BaseBlocks, BlockRegistry};
use crate::math::Vec3i;
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
    //
    // STUB: Player no longer lives on World — moved to `Game::player`
    // alongside the move of game_time, terrain gen, etc., out of the
    // database. The command system threads only `&mut World`, so
    // commands can't reach the player. Reactivating these commands
    // requires changing the Command signature to take a context
    // bundle (World + Player + DaylightCycle + BlockUpdateQueue).
    {
        let _ = block_registry;
        registry.add(
            "/kit",
            Command::new(move |args, _world, _messages| args.len() == 1),
        );
    }

    // /give <id> <amount> — STUB; see /kit.
    registry.add(
        "/give",
        Command::new(|args, _world, _messages| {
            args.len() == 3
                && parse_int::<u16>(args[1]).is_some()
                && parse_int::<u32>(args[2]).is_some()
        }),
    );

    // /tp <x> <y> <z> — STUB; see /kit.
    registry.add(
        "/tp",
        Command::new(|args, _world, _messages| {
            args.len() == 4
                && parse_float::<f64>(args[1]).is_some()
                && parse_float::<f64>(args[2]).is_some()
                && parse_float::<f64>(args[3]).is_some()
        }),
    );

    // /clearinventory — STUB; see /kit.
    registry.add(
        "/clearinventory",
        Command::new(|args, _world, _messages| args.len() == 1),
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
            // The command system threads `&mut World` only; stub the
            // queue here. Real block-update routing reactivates when
            // commands grow access to `Game`'s `BlockUpdateQueue`.
            let mut q = crate::core::game::block_update::BlockUpdateQueue::new();
            crate::core::game::block_update::set_block(
                world,
                &mut q,
                Vec3i::new(x, y, z),
                blocks::Id(id),
                true,
            );
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
            crate::core::game::block_update::build_tree(world, Vec3i::new(x, y, z));
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
            crate::core::game::block_update::explode(world, Vec3i::new(x, y, z), r);
            true
        }),
    );

    // /time <time> — STUB; game_time moved to `Game::daylight_cycle`.
    registry.add(
        "/time",
        Command::new(|args, _world, _messages| {
            args.len() == 2 && parse_int::<u32>(args[1]).is_some()
        }),
    );

    // /gamemode <mode> — STUB; player moved to `Game::player`.
    registry.add(
        "/gamemode",
        Command::new(|args, _world, _messages| {
            if args.len() != 2 {
                return false;
            }
            let Some(raw) = parse_int::<u32>(args[1]) else {
                return false;
            };
            let _: GameMode = match raw {
                0 => GameMode::Survival,
                1 => GameMode::Creative,
                _ => return false,
            };
            true
        }),
    );
}
