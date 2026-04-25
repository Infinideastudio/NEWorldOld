//! `NEWorld` — Rust port.
//!
//! See `docs/rust_migration.md` (in the repo root) for the migration plan.
//!
//! Top-level layout reflects the C++ package structure:
//!
//! * Foundation (no graphics/windowing deps): [`math`], [`config`], [`i18n`],
//!   [`input`], [`blocks`], [`items`].
//! * World model: [`worldgen`], [`chunks`], [`worlds`] (containing [`worlds::world`]
//!   and [`worlds::player`]), [`commands`].
//! * Graphics + application bring-up: [`gfx`], [`app`].

pub mod app;
pub mod blocks;
pub mod chunks;
pub mod commands;
pub mod config;
pub mod gfx;
pub mod i18n;
pub mod input;
pub mod items;
pub mod math;
pub mod particles;
pub mod setup;
pub mod worldgen;
pub mod worlds;
