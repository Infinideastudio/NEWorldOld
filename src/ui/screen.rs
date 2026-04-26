//! Screen stack — the composable UI layer that sits on top of the 3-D
//! game view.
//!
//! Inspired by the original C++ `gui/screen.hpp` stack, but reified as
//! a Rust trait so individual screens (hud, inventory, main menu, etc.)
//! are self-contained.

use egui::Context;

/// What a screen wants the app to do after this frame.
pub enum Transition {
    /// Stay on this screen.
    None,
    /// Push a new screen on top of the stack (ownership transfer).
    Push(Box<dyn Screen>),
    /// Pop this screen off the stack.
    Pop,
    /// Exit the application.
    Exit,
}

/// A single screen in the UI stack. Think of it as a "page" in the app.
///
/// Every frame, the stack calls [`Self::ui`] on the topmost screen.
/// The screen builds its egui widgets and returns a [`Transition`]
/// describing what should happen next.
pub trait Screen {
    /// Human-readable name for logging/debug.
    fn title(&self) -> &'static str;

    /// Build the egui UI for this screen.
    ///
    /// The screen receives the full egui [`Context`] and should build
    /// its UI inside a `CentralPanel`, `Window`, or any other egui
    /// container.
    fn ui(&mut self, ctx: &Context) -> Transition;
}

/// A stack of screens. The topmost screen receives input and is rendered.
pub struct ScreenStack {
    screens: Vec<Box<dyn Screen>>,
}

impl ScreenStack {
    #[must_use]
    pub fn new() -> Self {
        Self {
            screens: Vec::new(),
        }
    }

    /// Push a screen onto the stack.
    pub fn push(&mut self, screen: Box<dyn Screen>) {
        tracing::info!(screen = screen.title(), "push screen");
        self.screens.push(screen);
    }

    /// Pop the top screen. Returns `None` if the stack is empty.
    pub fn pop(&mut self) -> Option<Box<dyn Screen>> {
        let s = self.screens.pop();
        if let Some(ref screen) = s {
            tracing::info!(screen = screen.title(), "pop screen");
        }
        s
    }

    /// Borrow the top screen, if any.
    #[must_use]
    pub fn top(&self) -> Option<&dyn Screen> {
        self.screens.last().map(AsRef::as_ref)
    }

    /// Number of screens on the stack.
    #[must_use]
    pub fn len(&self) -> usize {
        self.screens.len()
    }

    /// Whether the stack is empty.
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.screens.is_empty()
    }

    /// Tick the top screen: run its egui UI, then process the transition.
    ///
    /// Returns `true` if the app should exit (an `Exit` transition was
    /// returned by a screen). `Pop` on the last screen returns to the game.
    pub fn tick(&mut self, ctx: &Context) -> bool {
        let Some(top) = self.screens.last_mut() else {
            return false;
        };
        match top.ui(ctx) {
            Transition::None => false,
            Transition::Push(s) => {
                self.push(s);
                false
            }
            Transition::Pop => {
                self.pop();
                false
            }
            Transition::Exit => true,
        }
    }
}

impl Default for ScreenStack {
    fn default() -> Self {
        Self::new()
    }
}
