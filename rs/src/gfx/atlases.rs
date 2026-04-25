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
//! Sampling is `Nearest` for both filter and mipmap (the voxel pixel-art
//! aesthetic), with `ClampToEdge` addressing. No mipmaps for now.

use std::path::{Path, PathBuf};

use thiserror::Error;

/// Number of UI background variants on disk (`background_0..background_5`).
pub const BACKGROUND_COUNT: usize = 6;

/// Bytes per pixel after decoding to RGBA8.
const RGBA_BPP: u32 = 4;

/// A 2D-array texture (vertical-strip atlas).
///
/// Holds `layers` square sub-textures stacked vertically in the source PNG.
#[derive(Debug)]
pub struct AtlasArray {
    pub texture: wgpu::Texture,
    pub view: wgpu::TextureView,
    pub layers: u32,
}

/// A single 2D texture (one PNG → one texture).
#[derive(Debug)]
pub struct Atlas2d {
    pub texture: wgpu::Texture,
    pub view: wgpu::TextureView,
    pub size: (u32, u32),
}

/// All texture atlases used by the renderer, plus the shared sampler.
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

        let block_diffuse = load_strip_array(
            device,
            queue,
            &blocks.join("diffuse.png"),
            wgpu::TextureFormat::Rgba8UnormSrgb,
            Some("block_diffuse"),
        )?;
        let block_normal = load_strip_array(
            device,
            queue,
            &blocks.join("normal.png"),
            wgpu::TextureFormat::Rgba8Unorm,
            Some("block_normal"),
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

        let sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("atlases_sampler"),
            address_mode_u: wgpu::AddressMode::ClampToEdge,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            address_mode_w: wgpu::AddressMode::ClampToEdge,
            mag_filter: wgpu::FilterMode::Nearest,
            min_filter: wgpu::FilterMode::Nearest,
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
        })
    }

    /// Shared sampler used by every atlas binding.
    #[must_use]
    pub fn sampler(&self) -> &wgpu::Sampler {
        &self.sampler
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
/// square sub-image.
fn load_strip_array(
    device: &wgpu::Device,
    queue: &wgpu::Queue,
    path: &Path,
    format: wgpu::TextureFormat,
    label: Option<&str>,
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

    let size = wgpu::Extent3d {
        width,
        height: width,
        depth_or_array_layers: layers,
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

    let layer_byte_len = (width * width * RGBA_BPP) as usize;
    for layer_index in 0..layers {
        let start = layer_index as usize * layer_byte_len;
        let end = start + layer_byte_len;
        let layer_bytes = &bytes[start..end];

        queue.write_texture(
            wgpu::TexelCopyTextureInfo {
                texture: &texture,
                mip_level: 0,
                origin: wgpu::Origin3d {
                    x: 0,
                    y: 0,
                    z: layer_index,
                },
                aspect: wgpu::TextureAspect::All,
            },
            layer_bytes,
            wgpu::TexelCopyBufferLayout {
                offset: 0,
                bytes_per_row: Some(width * RGBA_BPP),
                rows_per_image: Some(width),
            },
            wgpu::Extent3d {
                width,
                height: width,
                depth_or_array_layers: 1,
            },
        );
    }

    let view = texture.create_view(&wgpu::TextureViewDescriptor {
        label,
        dimension: Some(wgpu::TextureViewDimension::D2Array),
        ..Default::default()
    });

    Ok(AtlasArray {
        texture,
        view,
        layers,
    })
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
