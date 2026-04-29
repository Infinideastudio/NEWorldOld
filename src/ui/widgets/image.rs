//! Image — leaf element that paints a registered `egui::TextureId`,
//! using the same `BoxFit` + alignment shape as the C++
//! `ui::ImageBox` (`old/src/ui/controls/image_box.ixx`).
//!
//! Layout: takes the full constraint as the container size — like the
//! C++ original, the *container* fills the parent and the texture is
//! fit / aligned inside it according to [`BoxFit`] + [`Alignment`].
//!
//! Render: derives the fitted source-UV rect (for COVER cropping) and
//! the destination rect (for CONTAIN letterboxing) via [`apply_box_fit`],
//! then dispatches a single `Painter::image` call.

use egui::{Color32, Context, Pos2, Rect, TextureId, Ui};

use crate::ui::layout::{Alignment, Constraint, Element, Point, Size};

/// How an image fits its container. Same shape as the C++ `BoxFit` —
/// see <https://api.flutter.dev/flutter/painting/BoxFit.html>.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub enum BoxFit {
    /// Don't scale; clip to the smaller of (container, texture) on each
    /// axis. The image keeps its native size.
    #[default]
    None,
    /// Stretch to exactly fill the container, ignoring aspect ratio.
    Fill,
    /// Scale uniformly to be as large as possible while still fitting
    /// inside the container (letterboxes if aspect ratios differ).
    Contain,
    /// Scale uniformly to fully cover the container (crops if aspect
    /// ratios differ).
    Cover,
    /// Scale so the texture's width matches the container's; clip the
    /// height to the smaller of (container_height/scale, intrinsic_height).
    FitWidth,
    /// Scale so the texture's height matches the container's; clip the
    /// width similarly.
    FitHeight,
}

/// Direct port of the C++ `apply_box_fit`. Returns a tuple
/// `(fitted_inner_size, fitted_container_size)`:
/// * `fitted_inner_size` is the size of the source region inside the
///   texture that we ultimately sample. For COVER this is smaller than
///   the texture; for CONTAIN / FILL it equals the texture's size.
/// * `fitted_container_size` is the size of the destination rect inside
///   the container. For CONTAIN this is smaller than the container;
///   for COVER / FILL it equals the container's size.
#[must_use]
pub fn apply_box_fit(fit: BoxFit, inner: Size, container: Size) -> (Size, Size) {
    match fit {
        BoxFit::None => {
            let clipped = Size::new(
                inner.width.min(container.width),
                inner.height.min(container.height),
            );
            (clipped, clipped)
        }
        BoxFit::Fill => (inner, container),
        BoxFit::Contain => {
            let scale = (container.width / inner.width.max(1e-6))
                .min(container.height / inner.height.max(1e-6));
            (inner, Size::new(inner.width * scale, inner.height * scale))
        }
        BoxFit::Cover => {
            let scale = (container.width / inner.width.max(1e-6))
                .max(container.height / inner.height.max(1e-6));
            (
                Size::new(container.width / scale, container.height / scale),
                container,
            )
        }
        BoxFit::FitWidth => {
            let scale = container.width / inner.width.max(1e-6);
            let width = inner.width;
            let height = inner.height.min(container.height / scale.max(1e-6));
            (
                Size::new(width, height),
                Size::new(width * scale, height * scale),
            )
        }
        BoxFit::FitHeight => {
            let scale = container.height / inner.height.max(1e-6);
            let width = inner.width.min(container.width / scale.max(1e-6));
            let height = inner.height;
            (
                Size::new(width, height),
                Size::new(width * scale, height * scale),
            )
        }
    }
}

/// Image leaf — mirrors C++ `ui::ImageBox`.
///
/// `texture` is the egui-side handle returned by
/// [`crate::render::EguiRenderer::register_native_textures`]; `intrinsic`
/// is the source PNG's pixel size (used for aspect-ratio fit math).
pub struct Image {
    texture: TextureId,
    intrinsic: Size,
    fit: BoxFit,
    alignment: Alignment,
    tint: Color32,
    /// Container size cached by [`Self::layout`] and consumed by
    /// [`Self::show`].
    size: Size,
}

impl Image {
    /// Build an Image from a registered texture id and the source PNG's
    /// pixel dimensions.
    #[must_use]
    pub fn new(texture: TextureId, width: f32, height: f32) -> Self {
        Self {
            texture,
            intrinsic: Size::new(width, height),
            fit: BoxFit::default(),
            alignment: Alignment::TopLeft,
            tint: Color32::WHITE,
            size: Size::ZERO,
        }
    }

    /// Set the [`BoxFit`] mode. Defaults to [`BoxFit::None`] (mirrors
    /// the C++ `ImageBox::Args::fit` default).
    #[must_use]
    pub fn fit(mut self, fit: BoxFit) -> Self {
        self.fit = fit;
        self
    }

    /// Set the alignment of the image inside its container. Defaults to
    /// [`Alignment::TopLeft`] (mirrors the C++ default).
    #[must_use]
    pub fn alignment(mut self, alignment: Alignment) -> Self {
        self.alignment = alignment;
        self
    }

    /// Multiplicative tint on the sampled texels. Defaults to
    /// [`Color32::WHITE`] (passthrough).
    #[must_use]
    pub fn tint(mut self, tint: Color32) -> Self {
        self.tint = tint;
        self
    }
}

impl Element for Image {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        // C++ `ImageBox::layout` returns the full constraint — the image
        // is rendered inside this container; the inner-fit math runs at
        // render time.
        self.size = c.into_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        if self.size.width <= 0.0 || self.size.height <= 0.0 {
            return;
        }
        let inner = Size::new(self.intrinsic.width.max(1.0), self.intrinsic.height.max(1.0));
        let container = self.size;
        let (fitted_inner, fitted_container) = apply_box_fit(self.fit, inner, container);

        let (ax, ay) = self.alignment.fractions();

        // Source UV rect: in [0..1] coordinates over the texture. With
        // BoxFit::Cover the source is cropped — fitted_inner is smaller
        // than `inner` and the alignment fractions decide which slice
        // we keep.
        let inner_left = (inner.width - fitted_inner.width) * ax;
        let inner_top = (inner.height - fitted_inner.height) * ay;
        let uv_min = Pos2::new(inner_left / inner.width, inner_top / inner.height);
        let uv_max = Pos2::new(
            (inner_left + fitted_inner.width) / inner.width,
            (inner_top + fitted_inner.height) / inner.height,
        );

        // Destination rect inside the container. With BoxFit::Contain
        // the destination is letterboxed — fitted_container is smaller
        // than `container` and the alignment fractions decide where it
        // sits.
        let container_left = (container.width - fitted_container.width) * ax;
        let container_top = (container.height - fitted_container.height) * ay;
        let dst_min = Pos2::new(origin.x + container_left, origin.y + container_top);
        let dst_max = Pos2::new(
            dst_min.x + fitted_container.width,
            dst_min.y + fitted_container.height,
        );

        ui.painter().image(
            self.texture,
            Rect::from_min_max(dst_min, dst_max),
            Rect::from_min_max(uv_min, uv_max),
            self.tint,
        );
    }
}
