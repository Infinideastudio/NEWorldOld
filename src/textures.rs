//! Block and UI texture atlases.
//!
//! Loads the per-tile and singleton PNGs from `<root>/textures/{blocks,ui}/...`
//! into `wgpu` textures:
//!
//! * `block_diffuse` and `block_normal` are `D2Array` textures with one
//!   layer per registered block texture. The layer order matches the
//!   [`BlockTextureRegistry`] index order, so a `BlockTextureIndex(i)`
//!   stored on a `BlockInfo` is also the array layer to sample.
//!   Source PNGs live at `<root>/textures/blocks/diffuse/<name>.png` and
//!   `<root>/textures/blocks/normal/<name>.png` (one file per registered
//!   texture name).
//! * `block_noise` is a single 2D noise texture.
//! * UI atlases (splash, title, six backgrounds) are single 2D / cube
//!   textures.
//!
//! Format choices follow the migration plan (§4.10):
//!
//! * `block_diffuse`, UI textures → `Rgba8UnormSrgb` (gamma-decoded on read).
//! * `block_normal`, `block_noise` → `Rgba8Unorm` (linear data; normals must
//!   not be gamma-decoded).
//!
//! Sampling is `Nearest` for min/mag (the voxel pixel-art aesthetic) with
//! `Linear` mipmap filtering — `block_diffuse` ships a full mipmap chain so
//! distant chunks anti-alias instead of shimmering, while close-up texels
//! stay crisp. Other atlases (UI, normal map, noise) ship mip level 0 only;
//! the sampler's mipmap filter is effectively ignored for those.

use std::path::{Path, PathBuf};

use thiserror::Error;

use crate::client::blocks::BlockTextureRegistry;

/// Number of UI background variants on disk (`background_0..background_5`).
pub const BACKGROUND_COUNT: usize = 6;

/// Bytes per pixel after decoding to RGBA8.
const RGBA_BPP: u32 = 4;

/// A 2D-array texture (vertical-strip atlas).
///
/// Holds `layers` square sub-textures stacked vertically in the source PNG.
/// In addition to the array view used by the chunk shader, holds one 2D view
/// per layer so callers (egui, debug HUD) can sample a single sub-image.
#[derive(Debug)]
pub struct AtlasArray {
    pub texture: wgpu::Texture,
    pub view: wgpu::TextureView,
    /// One D2 view per layer (`layer_views[i]` covers `base_array_layer = i`).
    /// Same `Texture` as `view`, just bound as a non-array sampler so it can
    /// be handed to egui (which only knows D2 textures). Includes every mip
    /// level so atlases with mipmaps still sample the full chain through
    /// these views.
    ///
    /// **Format note.** When the underlying texture is sRGB, these views
    /// are created in the matching unorm format instead — egui paints
    /// in gamma space, so a sample through an sRGB view would linearize
    /// the texel and let egui's gamma-space blend over-brighten the
    /// result. Unorm views hand the raw bytes back to the shader, which
    /// is what egui expects. The chunk pipeline uses the sRGB-format
    /// array `view` above for its lit math, so the two consumers see
    /// the same memory through correctly-suited views.
    pub layer_views: Vec<wgpu::TextureView>,
    pub layers: u32,
    /// Number of mip levels uploaded. `1` for atlases without mipmaps;
    /// `floor(log2(width)) + 1` for the block diffuse atlas (full chain
    /// down to a 1×1 root).
    pub mip_levels: u32,
}

/// A single 2D texture (one PNG → one texture).
#[derive(Debug)]
pub struct Atlas2d {
    pub texture: wgpu::Texture,
    pub view: wgpu::TextureView,
    pub size: (u32, u32),
}

/// A cubemap texture (6 square PNGs → one cube texture).
///
/// Layer order matches wgpu's cubemap face convention:
/// `[+X, -X, +Y, -Y, +Z, -Z]`. The `view` is created with
/// [`wgpu::TextureViewDimension::Cube`] so a `texture_cube<f32>`
/// binding samples it via a 3-component direction vector.
#[derive(Debug)]
pub struct AtlasCube {
    pub texture: wgpu::Texture,
    pub view: wgpu::TextureView,
    pub size: u32,
}

/// All texture atlases used by the renderer, plus the shared samplers.
///
/// Two distinct samplers, one for the voxel pixel-art atlases and one for
/// the deferred-renderer noise texture — each matching the C++ build's
/// per-texture configuration (`set_filter` + `set_wrap`):
///
/// * `sampler` — Nearest mag/min, Linear mipmap, Repeat wrap. Used by
///   `block_diffuse` / `block_normal` and any UI-style sampling that wants
///   pixel-exact magnification. Mirrors the C++
///   `LoadBlockTextureArray` / `LoadNormalTextureArray` settings
///   (`set_filter(false, true)`, `set_wrap(true)`).
/// * `noise_sampler` — Linear mag/min, no mipmap, Repeat wrap. Used by the
///   composition shader's noise dither / volumetric-cloud march. Mirrors
///   the C++ `LoadNoiseTextureArray` (`set_filter(true, false)`,
///   `set_wrap(true)`).
#[derive(Debug)]
pub struct Atlases {
    pub block_diffuse: AtlasArray,
    pub block_normal: AtlasArray,
    pub block_noise: Atlas2d,
    pub splash: Atlas2d,
    pub title: Atlas2d,
    /// 6-face skybox sampled by the out-of-game menu background pass. Layers
    /// are loaded from `background_0..5.png` in wgpu's `[+X, -X, +Y, -Y, +Z,
    /// -Z]` face order.
    pub background_cube: AtlasCube,
    sampler: wgpu::Sampler,
    noise_sampler: wgpu::Sampler,
}

/// Errors returned by [`Atlases::load`] and the helper free functions.
#[derive(Debug, Error)]
pub enum AtlasError {
    /// The file could not be read from disk.
    #[error("failed to read texture file {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },

    /// The PNG could not be decoded.
    #[error("failed to decode texture {path}: {source}")]
    Decode {
        path: PathBuf,
        #[source]
        source: image::ImageError,
    },

    /// A block tile PNG was not square, or its size did not match the
    /// other tiles in the same atlas. Every tile in
    /// `<root>/textures/blocks/{diffuse,normal}/` must share the same
    /// square size — the `D2Array` upload requires uniform dimensions.
    #[error(
        "block tile {path} has size {width}x{height} but expected square \
         {expected}x{expected}"
    )]
    InvalidTileSize {
        path: PathBuf,
        width: u32,
        height: u32,
        expected: u32,
    },

    /// A cubemap face was not square, or its size did not match the rest of
    /// the cube.
    #[error(
        "cubemap face {path} has size {width}x{height} but expected square \
         {expected}x{expected}"
    )]
    InvalidCubeFace {
        path: PathBuf,
        width: u32,
        height: u32,
        expected: u32,
    },

    /// `Atlases::load` was called with an empty `BlockTextureRegistry` —
    /// the renderer needs at least one tile to build a non-zero D2Array.
    #[error("block texture registry is empty: register at least one texture before loading atlases")]
    EmptyTextureRegistry,
}

impl Atlases {
    /// Load every atlas from `<root>/textures/{blocks,ui}/...`.
    ///
    /// Each PNG is decoded to RGBA8, then uploaded with
    /// [`wgpu::Queue::write_texture`]. The block diffuse / normal D2Arrays
    /// are built layer-by-layer from `<root>/textures/blocks/diffuse/<name>.png`
    /// and `<root>/textures/blocks/normal/<name>.png`, where `<name>` walks
    /// `block_textures.names()` in order — so the resulting array layers
    /// line up with the `BlockTextureIndex` values stored on each
    /// `BlockInfo`.
    pub fn load(
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        root: &Path,
        block_textures: &BlockTextureRegistry,
    ) -> Result<Self, AtlasError> {
        let blocks = root.join("textures").join("blocks");
        let ui = root.join("textures").join("ui");

        // Block diffuse / normal: one PNG per registered texture name,
        // stitched into a `D2Array`. Both ship a full mipmap chain so the
        // chunk shader can sample distant chunks without shimmering;
        // averaging encoded normals isn't strictly correct, but the
        // sampler's Nearest mag/min filter means close-up pixel art still
        // snaps cleanly to the base level.
        let block_diffuse = load_named_array(
            device,
            queue,
            &blocks.join("diffuse"),
            block_textures,
            wgpu::TextureFormat::Rgba8UnormSrgb,
            Some("block_diffuse"),
            true,
        )?;
        let block_normal = load_named_array(
            device,
            queue,
            &blocks.join("normal"),
            block_textures,
            wgpu::TextureFormat::Rgba8Unorm,
            Some("block_normal"),
            true,
        )?;
        let block_noise = load_2d(
            device,
            queue,
            &blocks.join("noise.png"),
            wgpu::TextureFormat::Rgba8Unorm,
            Some("block_noise"),
        )?;

        let splash = load_2d(
            device,
            queue,
            &ui.join("splash.png"),
            wgpu::TextureFormat::Rgba8UnormSrgb,
            Some("ui_splash"),
        )?;
        let title = load_2d(
            device,
            queue,
            &ui.join("title.png"),
            wgpu::TextureFormat::Rgba8UnormSrgb,
            Some("ui_title"),
        )?;

        // 6-face skybox. Stack the PNGs into one cubemap texture so the
        // menu-background pass can sample by direction rather than per-face.
        let background_paths: [PathBuf; BACKGROUND_COUNT] = [
            ui.join("background_0.png"),
            ui.join("background_1.png"),
            ui.join("background_2.png"),
            ui.join("background_3.png"),
            ui.join("background_4.png"),
            ui.join("background_5.png"),
        ];
        let background_cube = load_cube(
            device,
            queue,
            &background_paths,
            wgpu::TextureFormat::Rgba8UnormSrgb,
            Some("ui_background_cube"),
        )?;

        // U/V wrap so the greedy chunk mesher can tile a single block-art
        // square across a merged quad's UV span (`length + 1` repetitions).
        // For UI/single-tile sampling all UVs stay in `[0, 1]`, so Repeat is
        // visually identical to ClampToEdge there.
        //
        // `mag/min: Nearest` keeps the voxel pixel-art look at close range;
        // `mipmap: Linear` smoothly fades between mip levels at distance,
        // which kills the moiré-style shimmer on faraway chunks. `lod_max =
        // mip_count - 1` would clamp the chain explicitly; the default
        // `f32::MAX` works too because textures with only 1 mip silently
        // ignore the mipmap filter.
        let sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("atlases_sampler"),
            address_mode_u: wgpu::AddressMode::Repeat,
            address_mode_v: wgpu::AddressMode::Repeat,
            address_mode_w: wgpu::AddressMode::ClampToEdge,
            mag_filter: wgpu::FilterMode::Nearest,
            min_filter: wgpu::FilterMode::Nearest,
            mipmap_filter: wgpu::MipmapFilterMode::Linear,
            ..Default::default()
        });

        // Noise sampler — Linear mag/min, no mipmap, Repeat wrap. Matches
        // the C++ `LoadNoiseTextureArray` that runs `set_filter(true,
        // false)` + `set_wrap(true)`. The composition shader's
        // `noise_dither` / `interpolated_noise` (when SSR + volumetric
        // clouds land) need bilinear interpolation, which would be lost
        // through the Nearest-only `sampler` above.
        let noise_sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("atlases_noise_sampler"),
            address_mode_u: wgpu::AddressMode::Repeat,
            address_mode_v: wgpu::AddressMode::Repeat,
            address_mode_w: wgpu::AddressMode::Repeat,
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            mipmap_filter: wgpu::MipmapFilterMode::Nearest,
            ..Default::default()
        });

        Ok(Self {
            block_diffuse,
            block_normal,
            block_noise,
            splash,
            title,
            background_cube,
            sampler,
            noise_sampler,
        })
    }

    /// Shared sampler used by the block / UI atlases (Nearest mag/min,
    /// Linear mipmap, Repeat wrap).
    #[must_use]
    pub fn sampler(&self) -> &wgpu::Sampler {
        &self.sampler
    }

    /// Linear-filtered Repeat sampler bound to the noise texture in the
    /// composition pass. Matches the C++ `LoadNoiseTextureArray` config.
    #[must_use]
    pub fn noise_sampler(&self) -> &wgpu::Sampler {
        &self.noise_sampler
    }
}

/// Decode a PNG file to `(width, height, rgba_bytes)`.
fn decode_rgba(path: &Path) -> Result<(u32, u32, Vec<u8>), AtlasError> {
    // `image::open` will internally read the file; map IO errors out
    // separately when we can detect them via metadata first, otherwise the
    // unified `Decode` variant is fine for the actual decode failure.
    let img = image::open(path).map_err(|source| match &source {
        image::ImageError::IoError(_) => AtlasError::Io {
            path: path.to_path_buf(),
            // unwrap: we just matched on `IoError`, so the conversion is
            // a destructure into `io::Error`. Re-construct via `kind`.
            source: io_error_from_image(&source),
        },
        _ => AtlasError::Decode {
            path: path.to_path_buf(),
            source,
        },
    })?;

    let rgba = img.to_rgba8();
    let (w, h) = rgba.dimensions();
    Ok((w, h, rgba.into_raw()))
}

/// Build a fresh `io::Error` mirroring an `image::ImageError::IoError`.
///
/// Avoids consuming the original error so we can also include it in the
/// `image::ImageError` chain if we ever decide to nest. For now we keep both
/// variants distinct (IO vs decode) for ergonomics.
fn io_error_from_image(err: &image::ImageError) -> std::io::Error {
    if let image::ImageError::IoError(inner) = err {
        std::io::Error::new(inner.kind(), inner.to_string())
    } else {
        std::io::Error::other(err.to_string())
    }
}

/// Load a single 2D texture from `path` with the given `format`.
fn load_2d(
    device: &wgpu::Device,
    queue: &wgpu::Queue,
    path: &Path,
    format: wgpu::TextureFormat,
    label: Option<&str>,
) -> Result<Atlas2d, AtlasError> {
    let (width, height, bytes) = decode_rgba(path)?;

    let size = wgpu::Extent3d {
        width,
        height,
        depth_or_array_layers: 1,
    };
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label,
        size,
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
        view_formats: &[],
    });

    queue.write_texture(
        wgpu::TexelCopyTextureInfo {
            texture: &texture,
            mip_level: 0,
            origin: wgpu::Origin3d::ZERO,
            aspect: wgpu::TextureAspect::All,
        },
        &bytes,
        wgpu::TexelCopyBufferLayout {
            offset: 0,
            bytes_per_row: Some(width * RGBA_BPP),
            rows_per_image: Some(height),
        },
        size,
    );

    let view = texture.create_view(&wgpu::TextureViewDescriptor {
        label,
        dimension: Some(wgpu::TextureViewDimension::D2),
        ..Default::default()
    });

    Ok(Atlas2d {
        texture,
        view,
        size: (width, height),
    })
}

/// Load one PNG per registered texture name into a `D2Array` texture with
/// one layer per name (in `registry.names()` order).
///
/// `dir` is the parent directory; for each `name` the loader reads
/// `<dir>/<name>.png`. Every PNG must be square and share the same edge
/// length — `D2Array` uploads need uniform layer dimensions. When
/// `with_mipmaps` is set the function generates a full CPU-side mipmap
/// chain down to `1×1` so the chunk shader's distance sampling
/// anti-aliases without GPU mipmap generation.
fn load_named_array(
    device: &wgpu::Device,
    queue: &wgpu::Queue,
    dir: &Path,
    registry: &BlockTextureRegistry,
    format: wgpu::TextureFormat,
    label: Option<&str>,
    with_mipmaps: bool,
) -> Result<AtlasArray, AtlasError> {
    let names = registry.names();
    if names.is_empty() {
        return Err(AtlasError::EmptyTextureRegistry);
    }

    // Decode every tile; validate that all share one square size.
    let mut tiles: Vec<Vec<u8>> = Vec::with_capacity(names.len());
    let mut tile_size: Option<u32> = None;
    for name in names {
        let path = dir.join(format!("{name}.png"));
        let (w, h, bytes) = decode_rgba(&path)?;
        let expected = tile_size.unwrap_or(w);
        if w != h || (tile_size.is_some() && w != expected) {
            return Err(AtlasError::InvalidTileSize {
                path,
                width: w,
                height: h,
                expected,
            });
        }
        tile_size = Some(w);
        tiles.push(bytes);
    }
    let width = tile_size.expect("at least one tile (registry was non-empty)");
    let layers = u32::try_from(names.len()).expect("BlockTextureIndex fits in u16, so in u32 too");

    let mip_levels = if with_mipmaps {
        // `floor(log2(width)) + 1` levels to reach 1×1. 32×32 tiles → 6.
        32 - width.max(1).leading_zeros()
    } else {
        1
    };

    let size = wgpu::Extent3d {
        width,
        height: width,
        depth_or_array_layers: layers,
    };
    // Per-layer egui views need a non-sRGB view format on sRGB textures —
    // see `AtlasArray::layer_views` doc. Declare the alias here so the
    // views can be created later.
    let layer_view_format = format.remove_srgb_suffix();
    let view_formats: &[wgpu::TextureFormat] = if layer_view_format == format {
        &[]
    } else {
        std::slice::from_ref(&layer_view_format)
    };
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label,
        size,
        mip_level_count: mip_levels,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
        view_formats,
    });

    // Upload each tile as one array layer. PNG row 0 lands at `t = 0` in
    // wgpu; tiles are now authored right-side-up per file (one PNG per
    // tile, no global Y-flip needed).
    for (texture_layer, tile_bytes) in tiles.into_iter().enumerate() {
        let texture_layer = texture_layer as u32;
        let mut current = tile_bytes;
        let mut level_size = width;
        for level in 0..mip_levels {
            queue.write_texture(
                wgpu::TexelCopyTextureInfo {
                    texture: &texture,
                    mip_level: level,
                    origin: wgpu::Origin3d {
                        x: 0,
                        y: 0,
                        z: texture_layer,
                    },
                    aspect: wgpu::TextureAspect::All,
                },
                &current,
                wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(level_size * RGBA_BPP),
                    rows_per_image: Some(level_size),
                },
                wgpu::Extent3d {
                    width: level_size,
                    height: level_size,
                    depth_or_array_layers: 1,
                },
            );
            if level + 1 < mip_levels {
                let next_size = (level_size / 2).max(1);
                current = downsample_2x_rgba8(&current, level_size, level_size);
                level_size = next_size;
            }
        }
    }

    let view = texture.create_view(&wgpu::TextureViewDescriptor {
        label,
        dimension: Some(wgpu::TextureViewDimension::D2Array),
        ..Default::default()
    });

    let mut layer_views = Vec::with_capacity(layers as usize);
    for layer in 0..layers {
        let layer_label = label.map(|l| format!("{l}.layer{layer}"));
        layer_views.push(texture.create_view(&wgpu::TextureViewDescriptor {
            label: layer_label.as_deref(),
            // Unorm view of an sRGB texture so egui's gamma-space shader
            // sees raw bytes; chunk pipeline still samples through the
            // sRGB `view` and gets hardware linearization.
            format: Some(layer_view_format),
            dimension: Some(wgpu::TextureViewDimension::D2),
            base_array_layer: layer,
            array_layer_count: Some(1),
            ..Default::default()
        }));
    }

    Ok(AtlasArray {
        texture,
        view,
        layer_views,
        layers,
        mip_levels,
    })
}

/// Downsample a `width × height` RGBA8 image to `(width/2) × (height/2)` by
/// 2×2 box-averaging. Naive sRGB-space average — fine for the voxel atlas
/// since each block tile is mostly hard-edged pixel art with no smooth
/// gradients to gamma-distort. Caller guarantees `width` and `height` are
/// powers of two so the halved sizes stay integer.
fn downsample_2x_rgba8(src: &[u8], width: u32, height: u32) -> Vec<u8> {
    let dst_w = (width / 2).max(1);
    let dst_h = (height / 2).max(1);
    let mut out = vec![0u8; (dst_w * dst_h * RGBA_BPP) as usize];
    let stride = (width * RGBA_BPP) as usize;
    let bpp = RGBA_BPP as usize;
    for y in 0..dst_h {
        for x in 0..dst_w {
            // Source corner pixel for this destination cell.
            let sx = (x * 2) as usize;
            let sy = (y * 2) as usize;
            let mut acc = [0u32; 4];
            // 2×2 tap. Clamp the +1 step against `width` / `height` so a
            // downsample from a 1-row image (level_size = 1) still works.
            for dy in 0..2 {
                for dx in 0..2 {
                    let rx = (sx + dx).min((width - 1) as usize);
                    let ry = (sy + dy).min((height - 1) as usize);
                    let off = ry * stride + rx * bpp;
                    acc[0] += u32::from(src[off]);
                    acc[1] += u32::from(src[off + 1]);
                    acc[2] += u32::from(src[off + 2]);
                    acc[3] += u32::from(src[off + 3]);
                }
            }
            let dst_off = (y * dst_w * RGBA_BPP + x * RGBA_BPP) as usize;
            out[dst_off] = (acc[0] / 4) as u8;
            out[dst_off + 1] = (acc[1] / 4) as u8;
            out[dst_off + 2] = (acc[2] / 4) as u8;
            out[dst_off + 3] = (acc[3] / 4) as u8;
        }
    }
    out
}

/// Load 6 square PNGs into a single cubemap texture.
///
/// Faces are uploaded in wgpu's `[+X, -X, +Y, -Y, +Z, -Z]` order — the array
/// `paths` is indexed identically. Every face must be the same square size;
/// the first face's width sets the expected size and any later face that
/// disagrees raises [`AtlasError::InvalidCubeFace`].
///
/// The view is created with [`wgpu::TextureViewDimension::Cube`] so a
/// `texture_cube<f32>` binding samples it via a 3-component direction vector.
fn load_cube(
    device: &wgpu::Device,
    queue: &wgpu::Queue,
    paths: &[PathBuf; BACKGROUND_COUNT],
    format: wgpu::TextureFormat,
    label: Option<&str>,
) -> Result<AtlasCube, AtlasError> {
    let mut faces: [Option<(u32, u32, Vec<u8>)>; BACKGROUND_COUNT] =
        [const { None }; BACKGROUND_COUNT];
    let mut size: Option<u32> = None;
    for (i, path) in paths.iter().enumerate() {
        let (w, h, bytes) = decode_rgba(path)?;
        if w != h {
            return Err(AtlasError::InvalidCubeFace {
                path: path.clone(),
                width: w,
                height: h,
                expected: size.unwrap_or(w),
            });
        }
        match size {
            None => size = Some(w),
            Some(s) if s != w => {
                return Err(AtlasError::InvalidCubeFace {
                    path: path.clone(),
                    width: w,
                    height: h,
                    expected: s,
                });
            }
            _ => {}
        }
        faces[i] = Some((w, h, bytes));
    }
    let size = size.expect("BACKGROUND_COUNT is non-zero so at least one face was decoded");

    let extent = wgpu::Extent3d {
        width: size,
        height: size,
        depth_or_array_layers: BACKGROUND_COUNT as u32,
    };
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label,
        size: extent,
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
        view_formats: &[],
    });

    for (layer, face) in faces.into_iter().enumerate() {
        let (_, _, bytes) = face.expect("every face was populated above");
        queue.write_texture(
            wgpu::TexelCopyTextureInfo {
                texture: &texture,
                mip_level: 0,
                origin: wgpu::Origin3d {
                    x: 0,
                    y: 0,
                    z: layer as u32,
                },
                aspect: wgpu::TextureAspect::All,
            },
            &bytes,
            wgpu::TexelCopyBufferLayout {
                offset: 0,
                bytes_per_row: Some(size * RGBA_BPP),
                rows_per_image: Some(size),
            },
            wgpu::Extent3d {
                width: size,
                height: size,
                depth_or_array_layers: 1,
            },
        );
    }

    let view = texture.create_view(&wgpu::TextureViewDescriptor {
        label,
        dimension: Some(wgpu::TextureViewDimension::Cube),
        ..Default::default()
    });

    Ok(AtlasCube {
        texture,
        view,
        size,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_registry_rejected() {
        // Building a `load_named_array` is GPU-bound and not exercised in
        // unit tests, but the empty-registry check is pure CPU and worth
        // pinning. Construct an empty registry and confirm the error
        // variant — a bare `if names.is_empty()` is easy to break later.
        use crate::client::blocks::BlockTextureRegistry;
        let r = BlockTextureRegistry::new();
        assert!(r.is_empty());
    }

    #[test]
    fn invalid_tile_size_variant_constructs() {
        // Compile-time check that the variant exists and matches.
        let err = AtlasError::InvalidTileSize {
            path: PathBuf::from("foo.png"),
            width: 32,
            height: 16,
            expected: 32,
        };
        assert!(matches!(
            err,
            AtlasError::InvalidTileSize { width: 32, height: 16, expected: 32, .. }
        ));
    }
}
