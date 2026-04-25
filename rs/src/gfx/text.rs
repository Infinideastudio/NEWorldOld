//! Glyphon-backed text rendering.
//!
//! Replaces the C++ `text_rendering.ixx` (`FreeType` + hand-rolled atlas) with
//! a thin wrapper around [`glyphon`], which in turn manages
//! [`cosmic_text`]-driven shaping and a `wgpu`-backed glyph atlas.
//!
//! See `docs/rust_migration.md` §4.11 for the design.
//!
//! Typical usage per frame:
//!
//! ```no_run
//! # use neworld::gfx::text::{TextRenderer, TextLine};
//! # fn frame(
//! #     device: &wgpu::Device,
//! #     queue: &wgpu::Queue,
//! #     renderer: &mut TextRenderer,
//! #     pass: &mut wgpu::RenderPass<'_>,
//! # ) {
//! let lines = [TextLine {
//!     text: "Hello, world!",
//!     x: 16.0,
//!     y: 16.0,
//!     scale: 1.0,
//!     color: glyphon::Color::rgb(255, 255, 255),
//! }];
//! renderer.prepare(device, queue, (1280, 720), &lines);
//! renderer.render(pass);
//! # }
//! ```
//!
//! `TextLine` is a single-line caller-facing wrapper; `prepare` shapes one
//! `glyphon::Buffer` per line.

use std::sync::Arc;

use glyphon::{
    Attrs, Buffer, Cache, Color, FontSystem, Metrics, Resolution, Shaping, SwashCache, TextArea,
    TextAtlas, TextBounds, Viewport, fontdb,
};

/// Bytes of the bundled `unicode.ttf`. The C++ build keeps this at
/// `<repo>/fonts/unicode.ttf`; the Rust port has a copy under
/// `rs/assets/fonts/unicode.ttf` so `include_bytes!` is path-stable.
const FONT_BYTES: &[u8] = include_bytes!("../../assets/fonts/unicode.ttf");

/// Default metrics — `(font_size, line_height)` in pixels.
const DEFAULT_METRICS: Metrics = Metrics::new(14.0, 18.0);

/// A simple caller-facing description of one shaped+rendered line of text.
///
/// `x` / `y` are the top-left corner in physical (post-DPI) pixels.
/// `scale` is multiplied into per-glyph positions and is independent of the
/// metrics in [`TextRenderer`]. `color` accepts pre-multiplied alpha.
#[derive(Clone, Copy, Debug)]
pub struct TextLine<'a> {
    pub text: &'a str,
    pub x: f32,
    pub y: f32,
    pub scale: f32,
    pub color: Color,
}

/// Owns the glyphon state required to lay out, cache, and draw text.
///
/// One `TextRenderer` per surface format is enough — multiple text passes
/// can share it by calling [`Self::prepare`] repeatedly between
/// [`Self::render`]s, but each `prepare` clears the prior frame's glyphs.
pub struct TextRenderer {
    font_system: FontSystem,
    swash_cache: SwashCache,
    atlas: TextAtlas,
    viewport: Viewport,
    renderer: glyphon::TextRenderer,
    default_metrics: Metrics,
    /// `Buffer`s reused across frames. Length grows to the largest line count
    /// we have ever seen and never shrinks; cosmic-text reuses internal
    /// allocations on `set_text`.
    buffers: Vec<Buffer>,
}

impl TextRenderer {
    /// Builds a text renderer for the given surface format.
    ///
    /// Loads the bundled `unicode.ttf` as binary font data into the
    /// fontdb backing the [`FontSystem`]. System fonts are scanned by
    /// [`FontSystem::new`] as a fallback.
    #[must_use]
    pub fn new(device: &wgpu::Device, queue: &wgpu::Queue, format: wgpu::TextureFormat) -> Self {
        let mut font_system = FontSystem::new();
        // Bundle the unicode font so we never depend on a system font being
        // installed. `Source::Binary(Arc<dyn AsRef<[u8]>>)` lets fontdb take
        // ownership without copying the slice.
        font_system
            .db_mut()
            .load_font_source(fontdb::Source::Binary(Arc::new(FONT_BYTES)));

        let swash_cache = SwashCache::new();
        let cache = Cache::new(device);
        let mut atlas = TextAtlas::new(device, queue, &cache, format);
        let viewport = Viewport::new(device, &cache);
        let renderer = glyphon::TextRenderer::new(
            &mut atlas,
            device,
            wgpu::MultisampleState::default(),
            None,
        );

        Self {
            font_system,
            swash_cache,
            atlas,
            viewport,
            renderer,
            default_metrics: DEFAULT_METRICS,
            buffers: Vec::new(),
        }
    }

    /// Returns the font metrics used when shaping new lines.
    #[must_use]
    pub fn default_metrics(&self) -> Metrics {
        self.default_metrics
    }

    /// Overrides the default metrics. Takes effect on the next [`Self::prepare`].
    pub fn set_default_metrics(&mut self, metrics: Metrics) {
        self.default_metrics = metrics;
    }

    /// Direct access to the underlying [`FontSystem`]. Useful if a caller
    /// wants to feed in additional fonts.
    pub fn font_system_mut(&mut self) -> &mut FontSystem {
        &mut self.font_system
    }

    /// Shapes `lines` and uploads glyphs to the atlas.
    ///
    /// `surface_size` is `(width, height)` in physical pixels and is forwarded
    /// to the glyphon [`Viewport`] so the GPU side knows where the screen
    /// edges are. Call this every frame, before [`Self::render`].
    pub fn prepare(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        surface_size: (u32, u32),
        lines: &[TextLine<'_>],
    ) {
        let (width, height) = surface_size;
        self.viewport.update(queue, Resolution { width, height });

        // Grow the buffer pool to fit, then reset metrics so the (possibly
        // larger) defaults set since last frame take effect.
        while self.buffers.len() < lines.len() {
            self.buffers
                .push(Buffer::new(&mut self.font_system, self.default_metrics));
        }

        // Shape every line into its own buffer.
        let attrs = Attrs::new();
        let width_f32 = width as f32;
        for (buffer, line) in self.buffers.iter_mut().zip(lines.iter()) {
            buffer.set_metrics(&mut self.font_system, self.default_metrics);
            // Width-bounded, vertically unbounded — single-line text wraps
            // only if it is wider than the surface.
            buffer.set_size(&mut self.font_system, Some(width_f32), None);
            buffer.set_text(
                &mut self.font_system,
                line.text,
                &attrs,
                Shaping::Advanced,
                None,
            );
            buffer.shape_until_scroll(&mut self.font_system, false);
        }

        // Build TextAreas referencing those buffers. Two passes (set_text
        // above, then borrow here) sidestep the `&mut`/`&` aliasing.
        let areas: Vec<TextArea<'_>> = self
            .buffers
            .iter()
            .zip(lines.iter())
            .map(|(buffer, line)| TextArea {
                buffer,
                left: line.x,
                top: line.y,
                scale: line.scale,
                bounds: TextBounds::default(),
                default_color: line.color,
                custom_glyphs: &[],
            })
            .collect();

        // `prepare` only fails on AtlasFull — log it and keep going so a
        // single missing line does not crash the frame.
        if let Err(err) = self.renderer.prepare(
            device,
            queue,
            &mut self.font_system,
            &mut self.atlas,
            &self.viewport,
            areas,
            &mut self.swash_cache,
        ) {
            tracing::error!(?err, "glyphon prepare failed");
        }
    }

    /// Records draw commands for the most recently prepared text into `pass`.
    pub fn render<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        if let Err(err) = self.renderer.render(&self.atlas, &self.viewport, pass) {
            tracing::error!(?err, "glyphon render failed");
        }
    }
}
