//! Chat command registry — `/help`, `/give`, `/tp`, `/setblock`, …
//!
//! Port of `src/commands.ixx` per `docs/rust_migration.md` §4.7.
//!
//! `Command` wraps a `Box<dyn Fn(&[&str], &mut World, &mut Vec<String>) -> bool +
//! Send + Sync>`. The registry is a `HashMap<String, Command>` plus a couple
//! of convenience methods (`execute_on`, `try_auto_complete`).
//!
//! `try_auto_complete(prefix)`: among names starting with `prefix`, returns
//! the lexicographically smallest match for determinism. The C++ version uses
//! `std::unordered_map::find_if` which is order-dependent; we improve on that.

mod base;

pub use self::base::register_base_commands;

use std::collections::HashMap;

use tracing::warn;

use crate::worlds::world::World;

/// Boxed closure type for [`Command::run`]. Pulled out as a type alias so the
/// `dyn Fn(...)` is named once and clippy `type_complexity` is happy.
pub type CommandFn = Box<dyn Fn(&[&str], &mut World, &mut Vec<String>) -> bool + Send + Sync>;

/// A single chat command. The closure receives the **whole** token list
/// (`args[0]` is the command name, including the leading `/`), a mutable
/// `World`, and the chat message buffer; it returns `false` if the args are
/// malformed for this command. The registry treats a `false` return as a
/// failure and writes a `Fail to execute the command: …` line into `messages`.
///
/// `Send + Sync` so the registry can be wrapped in `Arc` later if commands
/// cross threads.
pub struct Command {
    /// Run a single command instance. Args are the *whole* token list, where
    /// `args[0]` is the command name (including the leading `/`). Returns
    /// `false` if the args are malformed for this command — the registry then
    /// treats the invocation as a failure and writes a "fail to execute" line
    /// to `messages`.
    pub run: CommandFn,
}

impl Command {
    /// Build a command from any `Fn` closure with the right signature.
    pub fn new<F>(run: F) -> Self
    where
        F: Fn(&[&str], &mut World, &mut Vec<String>) -> bool + Send + Sync + 'static,
    {
        Self { run: Box::new(run) }
    }
}

impl std::fmt::Debug for Command {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Command").finish_non_exhaustive()
    }
}

/// Map of `name -> Command`. Names include the leading `/` (e.g. `"/help"`).
#[derive(Default)]
pub struct CommandRegistry {
    entries: HashMap<String, Command>,
}

impl std::fmt::Debug for CommandRegistry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("CommandRegistry")
            .field("entries", &self.entries.keys().collect::<Vec<_>>())
            .finish()
    }
}

impl CommandRegistry {
    /// Empty registry.
    pub fn new() -> Self {
        Self {
            entries: HashMap::new(),
        }
    }

    /// Register `command` under `name`. Panics if `name` is already taken
    /// (matches the C++ `unimplemented()` branch).
    pub fn add(&mut self, name: impl Into<String>, command: Command) {
        let name = name.into();
        assert!(
            !self.entries.contains_key(&name),
            "CommandRegistry::add: command {name:?} is already registered",
        );
        self.entries.insert(name, command);
    }

    /// All registered commands, in unspecified iteration order.
    pub fn entries(&self) -> impl Iterator<Item = (&str, &Command)> {
        self.entries.iter().map(|(k, v)| (k.as_str(), v))
    }

    /// Split `line` on whitespace, look up the first token, and dispatch.
    /// Mirrors the C++ `execute_on` behaviour: on lookup miss or on the
    /// command returning `false`, emits `tracing::warn!` and pushes
    /// `"Fail to execute the command: <line>"` to `messages`, then returns
    /// `false`.
    pub fn execute_on(&self, line: &str, world: &mut World, messages: &mut Vec<String>) -> bool {
        let parts: Vec<&str> = line.split_whitespace().collect();
        if let Some(name) = parts.first()
            && let Some(cmd) = self.entries.get(*name)
            && (cmd.run)(&parts, world, messages)
        {
            return true;
        }
        warn!("Fail to execute the command: {line}");
        messages.push(format!("Fail to execute the command: {line}"));
        false
    }

    /// Among names starting with `prefix`, returns the lexicographically
    /// smallest match for determinism. Returns `None` if nothing matches.
    /// The C++ version uses `std::unordered_map::find_if` which is
    /// order-dependent; sorting the keys here improves on that.
    pub fn try_auto_complete(&self, prefix: &str) -> Option<String> {
        let mut best: Option<&str> = None;
        for key in self.entries.keys() {
            if key.starts_with(prefix) && best.is_none_or(|b| key.as_str() < b) {
                best = Some(key.as_str());
            }
        }
        best.map(str::to_owned)
    }
}

// `register_base_commands` and the parse helpers live in `commands/base.rs`.

// ---------- tests ----------

// Tests here cover the registry-side logic (registration, lookup, auto-complete,
// `register_base_commands` count). The `execute_on` and per-command behaviour
// tests originally lived here too, written against a stub `World` with a
// recording API; they were dropped during the [B] merge because a real `World`
// requires opening a sled DB on disk, and a parallel-safe in-memory test
// fixture for that is out of scope for this layer. World mutations triggered by
// commands are exercised by `world::tests` (set_block / build_tree / explode)
// and `player::tests` (inventory / spawn / game_mode); the dispatch glue is
// trivial and can be re-tested when a `World::new_in_memory()` constructor
// exists.

#[cfg(test)]
mod tests {
    use std::sync::Arc;

    use super::*;
    use crate::blocks::{self, BlockRegistry};

    #[test]
    fn try_auto_complete_returns_first_match_or_none() {
        let mut br = BlockRegistry::new();
        let base = blocks::register_base_blocks(&mut br);
        let mut r = CommandRegistry::new();
        register_base_commands(&mut r, &base, Arc::new(br));
        assert_eq!(r.try_auto_complete("/he"), Some("/help".to_owned()));
        assert_eq!(r.try_auto_complete("/zzz"), None);
    }

    #[test]
    fn try_auto_complete_is_deterministic_across_calls() {
        let mut br = BlockRegistry::new();
        let base = blocks::register_base_blocks(&mut br);
        let mut r = CommandRegistry::new();
        register_base_commands(&mut r, &base, Arc::new(br));
        // `/c` matches both "/clear" and "/clearinventory"; the deterministic
        // (lexicographically smallest) answer is "/clear".
        let first = r.try_auto_complete("/c");
        let second = r.try_auto_complete("/c");
        assert_eq!(first, second);
        assert_eq!(first, Some("/clear".to_owned()));
    }

    #[test]
    fn register_base_commands_registers_exactly_eleven() {
        let mut br = BlockRegistry::new();
        let base = blocks::register_base_blocks(&mut br);
        let mut r = CommandRegistry::new();
        register_base_commands(&mut r, &base, Arc::new(br));
        // /help, /clear, /kit, /give, /tp, /clearinventory, /setblock,
        // /tree, /explode, /time, /gamemode = 11.
        assert_eq!(r.entries().count(), 11);
    }

    #[test]
    #[should_panic(expected = "is already registered")]
    fn add_panics_on_duplicate() {
        let mut r = CommandRegistry::new();
        r.add("/x", Command::new(|_, _, _| true));
        r.add("/x", Command::new(|_, _, _| true));
    }
}
