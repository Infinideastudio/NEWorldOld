//! Block and UI texture atlases.
//!
//! Loads the C++-era PNGs from `<root>/textures/{blocks,ui}/...` into `wgpu`
//! textures:
//!
//! * `block_diffuse` and `block_normal` are vertical strips of square
//!   sub-textures (width `W`, height `W * N`); they upload to a `D2Array`
//!   texture with `N` layers.
//! * `block_noise` is a single 2D noise texture.
//! * UI atlases (splash, title, select, unselect, six backgrounds) are
//!   single 2D textures.
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
    pub select: Atlas2d,
    pub unselect: Atlas2d,
    pub backgrounds: [Atlas2d; BACKGROUND_COUNT],
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

    /// A vertical-strip atlas had a height that is not a positive multiple of
    /// its width.
    #[error(
        "strip atlas {path} has invalid dimensions {width}x{height}: \
         height must be a positive multiple of width"
    )]
    InvalidStripDimensions {
        path: PathBuf,
        width: u32,
        height: u32,
    },
}

impl Atlases {
    /// Load every atlas from `<root>/textures/{blocks,ui}/...`.
    ///
    /// Each PNG is decoded to RGBA8, then uploaded with
    /// [`wgpu::Queue::write_texture`]. Strip atlases (`blocks/diffuse.png`,
    /// `blocks/normal.png`) are sliced into square layers; their height must
    /// be a positive multiple of their width.
    pub fn load(
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        root: &Path,
    ) -> Result<Self, AtlasError> {
        let blocks = root.join("textures").join("blocks");
        let ui = root.join("textures").join("ui");

        // Both diffuse and normal ship a full mipmap chain — matches the C++
        // `LoadBlockTextureArray` / `LoadNormalTextureArray`, both of which
        // call `generate_mipmaps()`. Mathematically averaging encoded
        // normals isn't ideal, but the sampler's Nearest mag/min filter
        // means base-level pixel art still snaps cleanly, and parity with
        // the C++ build matters more than a hypothetical normal-mapped
        // BRDF (which the deferred composition shader will pick up later).
        let block_diffuse = load_strip_array(
            device,
            queue,
            &blocks.join("diffuse.png"),
            wgpu::TextureFormat::Rgba8UnormSrgb,
            Some("block_diffuse"),
            true,
        )?;
        let block_normal = load_strip_array(
            device,
            queue,
            &blocks.join("normal.png"),
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
        let select = load_2d(
            device,
            queue,
            &ui.join("select.png"),
            wgpu::TextureFormat::Rgba8UnormSrgb,
            Some("ui_select"),
        )?;
        let unselect = load_2d(
            device,
            queue,
            &ui.join("unselect.png"),
            wgpu::TextureFormat::Rgba8UnormSrgb,
            Some("ui_unselect"),
        )?;

        // Six fixed backgrounds. The const-generic `[T; N]::try_from` dance
        // would require `Atlas2d: Copy`, which it is not (it owns `wgpu`
        // resources). Build the array explicitly via individual `?` calls.
        let bg = |i: usize| -> Result<Atlas2d, AtlasError> {
            let label = format!("ui_background_{i}");
            load_2d(
                device,
                queue,
                &ui.join(format!("background_{i}.png")),
                wgpu::TextureFormat::Rgba8UnormSrgb,
                Some(&label),
            )
        };
        let backgrounds: [Atlas2d; BACKGROUND_COUNT] =
            [bg(0)?, bg(1)?, bg(2)?, bg(3)?, bg(4)?, bg(5)?];

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
            select,
            unselect,
            backgrounds,
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

/// Compute the layer count for a vertical-strip atlas of dimensions
/// `width x height`.
///
/// Returns [`AtlasError::InvalidStripDimensions`] (with `path` set to an
/// empty path — caller is responsible for filling it in if needed) when the
/// height is zero or not divisible by the width. The caller-friendly wrapper
/// [`load_strip_array`] sets the proper `path` on its error returns.
pub fn compute_layer_count(width: u32, height: u32) -> Result<u32, AtlasError> {
    if width == 0 || height == 0 || !height.is_multiple_of(width) {
        return Err(AtlasError::InvalidStripDimensions {
            path: PathBuf::new(),
            width,
            height,
        });
    }
    Ok(height / width)
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

/// Load a vertical-strip PNG into a `D2Array` texture with one layer per
/// square sub-image. When `with_mipmaps` is set, generates a full mipmap
/// chain (CPU box-filter; gamma-naive, fine for opaque pixel art) down to
/// `1×1` so the chunk shader's distance sampling anti-aliases.
fn load_strip_array(
    device: &wgpu::Device,
    queue: &wgpu::Queue,
    path: &Path,
    format: wgpu::TextureFormat,
    label: Option<&str>,
    with_mipmaps: bool,
) -> Result<AtlasArray, AtlasError> {
    let (width, height, bytes) = decode_rgba(path)?;
    let layers = compute_layer_count(width, height).map_err(|e| match e {
        AtlasError::InvalidStripDimensions { width, height, .. } => {
            AtlasError::InvalidStripDimensions {
                path: path.to_path_buf(),
                width,
                height,
            }
        }
        // `compute_layer_count` only returns InvalidStripDimensions today,
        // but keep the explicit pass-through for future-proofing.
        other => other,
    })?;

    let mip_levels = if with_mipmaps {
        // `floor(log2(width)) + 1` levels to reach a 1×1 root. Block atlas
        // tiles are typically 32×32 → 6 levels. Layer width is the relevant
        // dimension because each strip block is square.
        32 - width.max(1).leading_zeros()
    } else {
        1
    };

    let size = wgpu::Extent3d {
        width,
        height: width,
        depth_or_array_layers: layers,
    };
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label,
        size,
        mip_level_count: mip_levels,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
        view_formats: &[],
    });

    // The C++ `render::load_png_image` Y-flips the PNG during decode (see
    // `src/render/image.ixx:197` — it walks rows from `height - 1` down to `0`
    // when reading), so the source PNGs are authored with the LAST atlas
    // entry at the top of the file and the FIRST at the bottom. Our Rust
    // decoder loads top-down (PNG row 0 → buffer row 0), so we need to map
    // `texture_layer[k] ← png_block[layers - 1 - k]` to match the C++
    // convention. Within each W×W block the row order is preserved
    // (PNG-visual top-to-bottom = wgpu's `t = 0` at top), so each block's art
    // appears right-side-up.
    let layer_byte_len = (width * width * RGBA_BPP) as usize;
    for texture_layer in 0..layers {
        let png_block = layers - 1 - texture_layer;
        let start = png_block as usize * layer_byte_len;
        let end = start + layer_byte_len;
        let level0: Vec<u8> = bytes[start..end].to_vec();

        // Walk the mip chain. `current` holds the `level`'s pixel buffer,
        // which we hand to `write_texture` and then downsample for the
        // next iteration.
        let mut current = level0;
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

#[cfg(test)]
mod tests {
    use super::{AtlasError, compute_layer_count};

    #[test]
    fn layer_count_divisible() {
        assert_eq!(compute_layer_count(32, 32).unwrap(), 1);
        assert_eq!(compute_layer_count(32, 64).unwrap(), 2);
        assert_eq!(compute_layer_count(32, 960).unwrap(), 30);
        assert_eq!(compute_layer_count(64, 64 * 17).unwrap(), 17);
    }

    #[test]
    fn layer_count_zero_width_rejected() {
        let err = compute_layer_count(0, 32).unwrap_err();
        assert!(matches!(
            err,
            AtlasError::InvalidStripDimensions { width: 0, height: 32, .. }
        ));
    }

    #[test]
    fn layer_count_zero_height_rejected() {
        let err = compute_layer_count(32, 0).unwrap_err();
        assert!(matches!(
            err,
            AtlasError::InvalidStripDimensions { width: 32, height: 0, .. }
        ));
    }

    #[test]
    fn layer_count_indivisible_rejected() {
        let err = compute_layer_count(32, 33).unwrap_err();
        assert!(matches!(
            err,
            AtlasError::InvalidStripDimensions { width: 32, height: 33, .. }
        ));
        let err = compute_layer_count(32, 100).unwrap_err();
        assert!(matches!(
            err,
            AtlasError::InvalidStripDimensions { width: 32, height: 100, .. }
        ));
    }
}
