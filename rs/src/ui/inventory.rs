//! Inventory overlay (E5).
//!
//! Provides a grid-based item view in a centered egui window.
//! The real `ItemStack` from `crate::items` will replace the placeholder
//! fields later.

/// Simple inventory item slot for display purposes.
///
/// The real `ItemStack` from `crate::items` will replace the placeholder
/// fields once the item system is wired in.
#[derive(Clone, Debug, Default)]
pub struct InventorySlot {
    /// Placeholder item name.
    pub name: &'static str,
    /// Stack count.
    pub count: u8,
}

/// Player inventory state.
#[derive(Default)]
pub struct Inventory {
    /// 4 rows x 10 columns of item slots.
    pub slots: [[InventorySlot; 10]; 4],
    /// Whether the inventory overlay is visible.
    pub open: bool,
}

impl Inventory {
    /// Toggle the inventory open/closed.
    pub fn toggle(&mut self) {
        self.open = !self.open;
    }

    /// Render the inventory window. Call from the game screen's `ui` method.
    ///
    /// The window is non-collapsible, non-resizable, 400x300 px, centered on
    /// screen. Each of the 40 slots is drawn as a 32x32 px grey square.
    pub fn render(&mut self, ctx: &egui::Context) {
        if !self.open {
            return;
        }

        egui::Window::new("Inventory")
            .collapsible(false)
            .resizable(false)
            .default_size([400.0, 300.0])
            .anchor(egui::Align2::CENTER_CENTER, [0.0, 0.0])
            .show(ctx, |ui| {
                for row in 0..4 {
                    ui.horizontal(|ui| {
                        for col in 0..10 {
                            self.render_slot(ui, row, col);
                        }
                    });
                }
            });
    }

    /// Draw a single inventory slot.
    fn render_slot(&mut self, ui: &mut egui::Ui, row: usize, col: usize) {
        let name = self.slots[row][col].name;
        let count = self.slots[row][col].count;

        let frame = egui::Frame::NONE
            .inner_margin(2.0)
            .fill(egui::Color32::from_gray(50))
            .stroke(egui::Stroke::new(1.0, egui::Color32::from_gray(100)));

        frame.show(ui, |ui| {
            ui.set_min_size(egui::vec2(32.0, 32.0));
            if count > 0 {
                ui.label(name);
                ui.label(format!("{count}"));
            }
        });
    }
}
