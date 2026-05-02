//! `register_base_commands` — registers the 11 base-game slash commands.
//!
//! Mirrors C++ `commands.ixx::register_base_commands` (12 commands)
//! minus `/tree`, which is not ported in this round.
//!
//! Each command closure receives a [`CommandContext`] with mutable
//! references to the player, the block-update queue, the daylight
//! clock, plus the read-only block registry and `BaseBlocks`. Block
//! mutations route through `block_update::set_block` so light
//! propagation kicks off automatically.

use std::str::FromStr;

use crate::core::blocks::BlockId;
use crate::core::game::block_update;
use crate::core::game::player::GameMode;
use crate::core::math::{Vec3d, Vec3i};
use crate::items::ItemStack;

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

/// Register the 11 base-game commands (C++ `register_base_commands`
/// minus `/tree`).
pub fn register_base_commands(registry: &mut CommandRegistry) {
    // /help
    registry.add(
        "/help",
        Command::new(|args, _ctx, messages| {
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
                "          /setblock <x> <y> <z> <id> | /explode <x> <y> <z> <radius> | /time <time> | /gamemode <mode>"
                    .to_owned(),
            );
            true
        }),
    );

    // /clear
    registry.add(
        "/clear",
        Command::new(|args, _ctx, messages| {
            if args.len() != 1 {
                return false;
            }
            messages.clear();
            true
        }),
    );

    // /kit — give the player a stack of every registered block.
    registry.add(
        "/kit",
        Command::new(|args, ctx, _messages| {
            if args.len() != 1 {
                return false;
            }
            for i in 0..ctx.registry.len() {
                let id = BlockId(i as u16);
                ctx.player
                    .add_item(ItemStack::new(id, ItemStack::MAX_COUNT));
            }
            true
        }),
    );

    // /give <id> <amount>
    registry.add(
        "/give",
        Command::new(|args, ctx, _messages| {
            if args.len() != 3 {
                return false;
            }
            let Some(id) = parse_int::<u16>(args[1]) else {
                return false;
            };
            let Some(amount) = parse_int::<u32>(args[2]) else {
                return false;
            };
            // C++ takes `size_t` and lets `add_item` cap; `ItemStack::count`
            // is `u8`. Saturating cast keeps oversized requests tidy and
            // matches the C++-equivalent end state (one MAX_COUNT stack
            // takes the slot; any overflow is silently lost).
            let count = u8::try_from(amount).unwrap_or(u8::MAX);
            ctx.player.add_item(ItemStack::new(BlockId(id), count));
            true
        }),
    );

    // /tp <x> <y> <z>
    registry.add(
        "/tp",
        Command::new(|args, ctx, _messages| {
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
            ctx.player.set_coord(Vec3d::new(x, y, z));
            true
        }),
    );

    // /clearinventory
    registry.add(
        "/clearinventory",
        Command::new(|args, ctx, _messages| {
            if args.len() != 1 {
                return false;
            }
            ctx.player.clear_inventory();
            true
        }),
    );

    // /suicide — respawn at the world spawn coord.
    registry.add(
        "/suicide",
        Command::new(|args, ctx, _messages| {
            if args.len() != 1 {
                return false;
            }
            ctx.player.spawn();
            true
        }),
    );

    // /setblock <x> <y> <z> <id>
    registry.add(
        "/setblock",
        Command::new(|args, ctx, _messages| {
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
            block_update::set_block(
                ctx.world,
                ctx.queue,
                ctx.base,
                ctx.registry,
                Vec3i::new(x, y, z),
                BlockId(id),
                true,
            );
            true
        }),
    );

    // /explode <x> <y> <z> <radius>
    registry.add(
        "/explode",
        Command::new(|args, ctx, _messages| {
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
            block_update::explode(
                ctx.world,
                ctx.queue,
                ctx.base,
                ctx.registry,
                Vec3i::new(x, y, z),
                r,
            );
            true
        }),
    );

    // /time <time>
    registry.add(
        "/time",
        Command::new(|args, ctx, _messages| {
            if args.len() != 2 {
                return false;
            }
            let Some(t) = parse_int::<i32>(args[1]) else {
                return false;
            };
            if t < 0 {
                return false;
            }
            ctx.daylight.set_game_time(t as u32);
            true
        }),
    );

    // /gamemode <mode>
    registry.add(
        "/gamemode",
        Command::new(|args, ctx, _messages| {
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
            ctx.player.set_game_mode(mode);
            true
        }),
    );

}
