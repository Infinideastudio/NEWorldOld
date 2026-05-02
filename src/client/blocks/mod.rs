//! Client-side block visuals — the render-only counterpart to
//! `core::blocks`. Texture indices, per-block face textures, and the
//! base-game visual registration helper live here.
//!
//! A headless server can be built without depending on this module at all
//! — no GPU, no atlas, no texture names.

mod base;
mod info;
mod texture;

pub use base::register_base_block_visuals;
pub use info::{BlockRenderInfo, BlockRenderRegistry};
pub use texture::{BlockTextureIndex, BlockTextureRegistry};
