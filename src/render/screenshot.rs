//! Screenshot / thumbnail readback ([F4] in `docs/rust_migration.md` §5).
//!
//! Captures the current swap-chain image by issuing a
//! `copy_texture_to_buffer` into a `MAP_READ | COPY_DST` staging buffer, then
//! mapping the buffer asynchronously on a background thread and encoding the
//! pixels to PNG. The main thread is never blocked on the GPU — `capture`
//! only enqueues; the worker thread polls the device and runs the PNG
//! encode.
//!
//! Surface usage must include `COPY_SRC` (set in [`crate::render::context::Gfx`]
//! at startup); if the platform did not honor that, `capture` returns
//! [`ScreenshotError::SurfaceNotCopySrc`] and the caller logs + skips.
//!
//! Surface format must be `Bgra8UnormSrgb` (the only swap format we
//! configure on Windows). The B↔R swizzle is performed during PNG encode so
//! the bytes saved match standard sRGB ordering.

use std::path::PathBuf;
use std::sync::mpsc;
use std::thread;

use thiserror::Error;

/// `wgpu` requires a 256-byte alignment for the `bytes_per_row` field of a
/// buffer-copy operation. Re-export to avoid magic numbers in the math.
const ROW_ALIGNMENT: u32 = wgpu::COPY_BYTES_PER_ROW_ALIGNMENT;

/// Bytes per pixel for `Bgra8UnormSrgb` / `Rgba8UnormSrgb`.
const BYTES_PER_PIXEL: u32 = 4;

/// Errors produced by [`Screenshot::capture`].
#[derive(Debug, Error)]
pub enum ScreenshotError {
    /// The surface was not configured with `COPY_SRC` usage. The caller
    /// should log this once and skip — see [`crate::render::context::Gfx::new`]
    /// for the fallback path that turns this on.
    #[error("surface texture lacks COPY_SRC usage")]
    SurfaceNotCopySrc,

    /// We only support `Bgra8UnormSrgb` (the format we configure on Windows).
    /// Any other format would need a different swizzle path.
    #[error("unsupported surface format: {0:?}")]
    UnsupportedFormat(wgpu::TextureFormat),

    /// The capture target was zero-width or zero-height (typically a
    /// minimized window).
    #[error("zero-sized capture: {0}x{1}")]
    ZeroSize(u32, u32),
}

/// One-shot screenshot capturer.
///
/// Caches the staging buffer between captures (per-resolution): if the
/// surface size hasn't changed since the last capture, the buffer is
/// reused. The buffer itself is `Clone` (it's an `Arc` internally), so
/// shipping it to the worker thread is cheap.
pub struct Screenshot {
    cached: Option<CachedBuffer>,
}

struct CachedBuffer {
    buffer: wgpu::Buffer,
    width: u32,
    height: u32,
}

impl Default for Screenshot {
    fn default() -> Self {
        Self::new()
    }
}

impl Screenshot {
    /// Create a screenshot capturer with no cached buffer.
    pub fn new() -> Self {
        Self { cached: None }
    }

    /// Issue a copy from `surface_texture` into a mappable buffer in
    /// `encoder`, and spawn a worker that maps the buffer + writes a PNG
    /// to `output_path` after the next `queue.submit`.
    ///
    /// Caller is expected to call this *before* `queue.submit` (so the
    /// copy ends up in the same submission as the world/UI passes), then
    /// `queue.submit` and `frame.present` as usual. The worker thread
    /// polls the device on its own — the main thread never waits.
    ///
    /// # Errors
    ///
    /// * [`ScreenshotError::SurfaceNotCopySrc`] — surface usage doesn't
    ///   include `COPY_SRC` (the runtime fallback in `Gfx::new`).
    /// * [`ScreenshotError::UnsupportedFormat`] — format is not
    ///   `Bgra8UnormSrgb`.
    /// * [`ScreenshotError::ZeroSize`] — width or height is zero.
    pub fn capture(
        &mut self,
        device: &wgpu::Device,
        encoder: &mut wgpu::CommandEncoder,
        surface_texture: &wgpu::Texture,
        format: wgpu::TextureFormat,
        size: (u32, u32),
        output_path: PathBuf,
    ) -> Result<(), ScreenshotError> {
        let (width, height) = size;
        if width == 0 || height == 0 {
            return Err(ScreenshotError::ZeroSize(width, height));
        }
        if format != wgpu::TextureFormat::Bgra8UnormSrgb {
            return Err(ScreenshotError::UnsupportedFormat(format));
        }
        if !surface_texture
            .usage()
            .contains(wgpu::TextureUsages::COPY_SRC)
        {
            return Err(ScreenshotError::SurfaceNotCopySrc);
        }

        let padded_bytes_per_row = padded_bytes_per_row(width);
        let buffer_size = u64::from(padded_bytes_per_row) * u64::from(height);

        // Reuse the cached buffer if dimensions are unchanged.
        let buffer = match self.cached.as_ref() {
            Some(c) if c.width == width && c.height == height => c.buffer.clone(),
            _ => {
                let buf = device.create_buffer(&wgpu::BufferDescriptor {
                    label: Some("neworld.screenshot_staging"),
                    size: buffer_size,
                    usage: wgpu::BufferUsages::MAP_READ | wgpu::BufferUsages::COPY_DST,
                    mapped_at_creation: false,
                });
                self.cached = Some(CachedBuffer {
                    buffer: buf.clone(),
                    width,
                    height,
                });
                buf
            }
        };

        encoder.copy_texture_to_buffer(
            wgpu::TexelCopyTextureInfo {
                texture: surface_texture,
                mip_level: 0,
                origin: wgpu::Origin3d::ZERO,
                aspect: wgpu::TextureAspect::All,
            },
            wgpu::TexelCopyBufferInfo {
                buffer: &buffer,
                layout: wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(padded_bytes_per_row),
                    rows_per_image: Some(height),
                },
            },
            wgpu::Extent3d {
                width,
                height,
                depth_or_array_layers: 1,
            },
        );

        // The worker owns clones of `device` (for polling) and `buffer`
        // (for mapping). Both are Arc-backed handles, so this is cheap.
        let device = device.clone();
        let path = output_path;
        thread::spawn(move || {
            if let Err(err) =
                run_worker(&device, &buffer, width, height, padded_bytes_per_row, &path)
            {
                tracing::error!(error = %err, ?path, "screenshot worker failed");
            } else {
                tracing::info!(?path, "screenshot saved");
            }
        });

        Ok(())
    }
}

/// Compute `bytes_per_row` aligned up to `wgpu::COPY_BYTES_PER_ROW_ALIGNMENT`.
///
/// Public for unit tests; trivial otherwise.
pub fn padded_bytes_per_row(width: u32) -> u32 {
    let unaligned = width * BYTES_PER_PIXEL;
    let rem = unaligned % ROW_ALIGNMENT;
    if rem == 0 {
        unaligned
    } else {
        unaligned + (ROW_ALIGNMENT - rem)
    }
}

/// Per-pixel BGRA → RGBA swizzle. Public for unit tests.
fn swizzle_bgra_to_rgba(bgra: &[u8], rgba: &mut [u8]) {
    debug_assert_eq!(bgra.len(), rgba.len());
    debug_assert!(bgra.len().is_multiple_of(4));
    for (src, dst) in bgra.chunks_exact(4).zip(rgba.chunks_exact_mut(4)) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = src[3];
    }
}

/// Worker body: poll until the buffer is mapped, copy + swizzle pixels,
/// strip per-row padding, encode PNG.
fn run_worker(
    device: &wgpu::Device,
    buffer: &wgpu::Buffer,
    width: u32,
    height: u32,
    padded_bytes_per_row: u32,
    path: &std::path::Path,
) -> Result<(), WorkerError> {
    let (tx, rx) = mpsc::channel();
    buffer.slice(..).map_async(wgpu::MapMode::Read, move |res| {
        // Send result; ignore disconnects (the receiver is on this thread,
        // it'll only disconnect if we panicked).
        let _ = tx.send(res);
    });

    // Poll until the callback fires. `Wait` is fine here — we're not on
    // the main thread.
    loop {
        let _ = device.poll(wgpu::PollType::wait_indefinitely());
        match rx.try_recv() {
            Ok(Ok(())) => break,
            Ok(Err(err)) => return Err(WorkerError::Map(err)),
            Err(mpsc::TryRecvError::Empty) => {}
            Err(mpsc::TryRecvError::Disconnected) => {
                return Err(WorkerError::ChannelDisconnected);
            }
        }
    }

    // Mapped: copy bytes out, strip row padding, swizzle BGRA → RGBA.
    let unpadded_bpr = (width * BYTES_PER_PIXEL) as usize;
    let mut rgba = vec![0u8; unpadded_bpr * height as usize];
    {
        let view = buffer.slice(..).get_mapped_range();
        let src = &view[..];
        for y in 0..height as usize {
            let src_off = y * padded_bytes_per_row as usize;
            let dst_off = y * unpadded_bpr;
            swizzle_bgra_to_rgba(
                &src[src_off..src_off + unpadded_bpr],
                &mut rgba[dst_off..dst_off + unpadded_bpr],
            );
        }
        // `view` borrows the buffer; drop it before we unmap.
    }
    buffer.unmap();

    if let Some(parent) = path.parent()
        && !parent.as_os_str().is_empty()
    {
        std::fs::create_dir_all(parent).map_err(WorkerError::Io)?;
    }

    let img: image::ImageBuffer<image::Rgba<u8>, Vec<u8>> =
        image::ImageBuffer::from_raw(width, height, rgba).ok_or(WorkerError::EncodeBuffer)?;
    img.save_with_format(path, image::ImageFormat::Png)
        .map_err(WorkerError::Encode)?;
    Ok(())
}

#[derive(Debug, Error)]
enum WorkerError {
    #[error("buffer map_async failed: {0}")]
    Map(wgpu::BufferAsyncError),
    #[error("device poll channel disconnected")]
    ChannelDisconnected,
    #[error("io error: {0}")]
    Io(std::io::Error),
    #[error("PNG encode buffer construction failed (size mismatch)")]
    EncodeBuffer,
    #[error("PNG encode failed: {0}")]
    Encode(image::ImageError),
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn padded_bytes_per_row_matches_alignment() {
        // 256-byte alignment, 4 bytes per pixel.
        assert_eq!(padded_bytes_per_row(1), 256); // 4 -> 256
        assert_eq!(padded_bytes_per_row(63), 256); // 252 -> 256
        assert_eq!(padded_bytes_per_row(64), 256); // 256 -> 256 (exact)
        assert_eq!(padded_bytes_per_row(65), 512); // 260 -> 512
        assert_eq!(padded_bytes_per_row(128), 512); // 512 -> 512
        assert_eq!(padded_bytes_per_row(129), 768); // 516 -> 768
        assert_eq!(padded_bytes_per_row(1280), 5120); // exact
        assert_eq!(padded_bytes_per_row(1281), 5376); // 5124 -> 5376
        assert_eq!(padded_bytes_per_row(1920), 7680); // exact
        assert_eq!(padded_bytes_per_row(1919), 7680); // 7676 -> 7680
    }

    #[test]
    fn padded_bytes_per_row_is_multiple_of_alignment() {
        for w in [1u32, 2, 7, 100, 333, 1000, 1234, 4096] {
            let p = padded_bytes_per_row(w);
            assert!(
                p.is_multiple_of(ROW_ALIGNMENT),
                "width={w} padded={p} not aligned"
            );
            assert!(p >= w * BYTES_PER_PIXEL);
            assert!(p < w * BYTES_PER_PIXEL + ROW_ALIGNMENT);
        }
    }

    #[test]
    fn swizzle_bgra_to_rgba_swaps_b_and_r() {
        // (B, G, R, A) → (R, G, B, A).
        let bgra = [
            0x10, 0x20, 0x30, 0xFF, // pixel 0: B=10 G=20 R=30 A=FF
            0x40, 0x50, 0x60, 0x80, // pixel 1: B=40 G=50 R=60 A=80
        ];
        let mut rgba = [0u8; 8];
        swizzle_bgra_to_rgba(&bgra, &mut rgba);
        assert_eq!(
            rgba,
            [
                0x30, 0x20, 0x10, 0xFF, // R=30 G=20 B=10 A=FF
                0x60, 0x50, 0x40, 0x80, // R=60 G=50 B=40 A=80
            ]
        );
    }

    #[test]
    fn swizzle_preserves_alpha_and_swaps_channels() {
        // src bytes [B, G, R, A] = [0xAA, 0xBB, 0xCC, 0x77].
        let bgra = [0xAA, 0xBB, 0xCC, 0x77];
        let mut rgba = [0u8; 4];
        swizzle_bgra_to_rgba(&bgra, &mut rgba);
        // Output is [R, G, B, A] = [0xCC, 0xBB, 0xAA, 0x77].
        assert_eq!(rgba, [0xCC, 0xBB, 0xAA, 0x77]);
    }
}
