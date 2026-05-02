//! Client-only support modules.
//!
//! Anything under `client/` is the rendering / display half of the engine
//! and is **not** linked into a future headless server build. Block-render
//! info, texture registries, mesh caches, and atlas loaders all belong
//! here. The split mirrors `core/` (server-safe simulation) — `core` types
//! never reference `client` types, so the dependency arrow only points
//! one way.

pub mod blocks;
pub mod game;
pub mod menus;
pub mod render;
pub mod ui;
