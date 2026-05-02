//! Per-frame input snapshot consumed by the game logic.
//!
//! Pure data — no `winit` dependency. A future `app` module is responsible
//! for translating window events into [`InputState`] mutations. The lifecycle
//! mirrors the C++ `ui::Context::update_from` pattern: at the start of each
//! frame the producer calls [`InputState::begin_frame`] to clear the
//! per-frame transient fields, then folds in events for the new frame.

use std::ops::{BitAnd, BitOr, BitOrAssign};

use crate::math::Vec2f;

/// Mouse buttons. The discriminants match the bit positions used by
/// [`MouseButtons`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
#[repr(u8)]
pub enum MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4,
}

impl MouseButton {
    const ALL: [MouseButton; 5] = [
        MouseButton::Left,
        MouseButton::Right,
        MouseButton::Middle,
        MouseButton::X1,
        MouseButton::X2,
    ];

    fn mask(self) -> u8 {
        1 << (self as u8)
    }
}

/// Bitset of mouse buttons.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Hash)]
pub struct MouseButtons(u8);

impl MouseButtons {
    /// An empty set.
    pub const fn empty() -> Self {
        Self(0)
    }

    /// Whether `button` is in the set.
    pub fn contains(self, button: MouseButton) -> bool {
        (self.0 & button.mask()) != 0
    }

    /// Insert `button` into the set.
    pub fn insert(&mut self, button: MouseButton) {
        self.0 |= button.mask();
    }

    /// Remove `button` from the set.
    pub fn remove(&mut self, button: MouseButton) {
        self.0 &= !button.mask();
    }

    /// Clear every button from the set.
    pub fn clear(&mut self) {
        self.0 = 0;
    }

    /// True if the set contains no buttons.
    pub fn is_empty(self) -> bool {
        self.0 == 0
    }

    /// Iterate over the buttons currently in the set.
    pub fn iter(self) -> impl Iterator<Item = MouseButton> {
        MouseButton::ALL
            .into_iter()
            .filter(move |b| self.contains(*b))
    }
}

impl BitOr for MouseButtons {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self {
        Self(self.0 | rhs.0)
    }
}

impl BitAnd for MouseButtons {
    type Output = Self;
    fn bitand(self, rhs: Self) -> Self {
        Self(self.0 & rhs.0)
    }
}

impl BitOrAssign for MouseButtons {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

/// Logical keys we currently care about for gameplay and menus.
///
/// Mirrors the `GLFW_KEY_*` constants referenced in the C++ `neworld.ixx`
/// god file plus a few that the menu layer needs. `#[non_exhaustive]` so
/// adding more keys later is non-breaking. The discriminants double as
/// indices into the [`KeySet`] backing array — keep them stable.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
#[repr(u8)]
pub enum Key {
    // Letters used by gameplay
    A = 0,
    D = 1,
    E = 2,
    F = 3,
    G = 4,
    H = 5,
    L = 6,
    M = 7,
    R = 8,
    S = 9,
    W = 10,
    X = 11,
    Z = 12,
    // Punctuation
    Slash = 13,
    // Whitespace / editing
    Space = 14,
    Tab = 15,
    Enter = 16,
    Backspace = 17,
    Escape = 18,
    // Modifiers
    LeftShift = 19,
    RightShift = 20,
    LeftControl = 21,
    RightControl = 22,
    LeftAlt = 23,
    RightAlt = 24,
    // Function keys
    F1 = 25,
    F2 = 26,
    F3 = 27,
    F4 = 28,
    F5 = 29,
    F6 = 30,
    F7 = 31,
    F8 = 32,
    // Arrows (menus)
    ArrowLeft = 33,
    ArrowRight = 34,
    ArrowUp = 35,
    ArrowDown = 36,
    // Window-management keys
    F11 = 37,
}

impl Key {
    /// Reserve a few extra slots for future additions without resizing the
    /// backing array. Bumping this only invalidates serialized keysets, not
    /// public API. Must be a multiple of 64.
    const CAPACITY: usize = 128;

    fn index(self) -> usize {
        self as usize
    }
}

/// Bitset over [`Key`] values, backed by a fixed-size `u64` array sized to
/// hold every variant of [`Key`] (plus headroom).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct KeySet {
    bits: [u64; Self::WORDS],
}

impl KeySet {
    const WORDS: usize = Key::CAPACITY / 64;

    /// An empty set.
    pub const fn empty() -> Self {
        Self {
            bits: [0; Self::WORDS],
        }
    }

    /// Whether `key` is in the set.
    pub fn contains(&self, key: Key) -> bool {
        let i = key.index();
        (self.bits[i / 64] >> (i % 64)) & 1 == 1
    }

    /// Insert `key` into the set. Returns true if it was newly inserted.
    pub fn insert(&mut self, key: Key) -> bool {
        let was = self.contains(key);
        let i = key.index();
        self.bits[i / 64] |= 1u64 << (i % 64);
        !was
    }

    /// Remove `key` from the set. Returns true if it was previously set.
    pub fn remove(&mut self, key: Key) -> bool {
        let was = self.contains(key);
        let i = key.index();
        self.bits[i / 64] &= !(1u64 << (i % 64));
        was
    }

    /// Clear every key from the set.
    pub fn clear(&mut self) {
        self.bits = [0; Self::WORDS];
    }

    /// True if the set contains no keys.
    pub fn is_empty(&self) -> bool {
        self.bits.iter().all(|w| *w == 0)
    }
}

impl Default for KeySet {
    fn default() -> Self {
        Self::empty()
    }
}

/// Per-frame input snapshot. See module docs for the lifecycle.
///
/// `Default` is implemented by hand because `cgmath::Vector2` does not derive it.
#[derive(Debug, Clone)]
pub struct InputState {
    /// Cursor position this frame, in physical (window) pixels.
    pub mouse_pos: Vec2f,
    /// Cursor delta since last frame (computed by the producer of `InputState`).
    pub mouse_motion: Vec2f,
    /// Mouse wheel delta accumulated since last frame.
    pub mouse_wheel_delta: f32,
    /// Bitset of currently held mouse buttons (bit 0 = left, 1 = right, 2 = middle, ...).
    pub mouse_buttons: MouseButtons,
    /// Mouse buttons that were pressed (transitioned 0 -> 1) this frame.
    pub mouse_buttons_pressed: MouseButtons,
    /// Mouse buttons that were released (transitioned 1 -> 0) this frame.
    pub mouse_buttons_released: MouseButtons,
    /// Currently held keys.
    pub keys_down: KeySet,
    /// Keys whose state went from up -> down this frame.
    pub keys_pressed: KeySet,
    /// Keys whose state went from down -> up this frame.
    pub keys_released: KeySet,
    /// Text input characters captured this frame (UTF-8).
    pub text_input: String,
    /// Whether a backspace was pressed/repeated this frame.
    pub backspace: bool,
    /// True if the user requested window close.
    pub window_close_requested: bool,
}

impl Default for InputState {
    fn default() -> Self {
        Self {
            mouse_pos: Vec2f::new(0.0, 0.0),
            mouse_motion: Vec2f::new(0.0, 0.0),
            mouse_wheel_delta: 0.0,
            mouse_buttons: MouseButtons::empty(),
            mouse_buttons_pressed: MouseButtons::empty(),
            mouse_buttons_released: MouseButtons::empty(),
            keys_down: KeySet::empty(),
            keys_pressed: KeySet::empty(),
            keys_released: KeySet::empty(),
            text_input: String::new(),
            backspace: false,
            window_close_requested: false,
        }
    }
}

impl InputState {
    /// Construct an empty snapshot.
    pub fn new() -> Self {
        Self::default()
    }

    /// Mirrors the C++ `isKeyDown`.
    pub fn is_key_down(&self, key: Key) -> bool {
        self.keys_down.contains(key)
    }

    /// Mirrors the C++ `isKeyPressed` — true on the frame the key transitioned
    /// up -> down.
    pub fn is_key_pressed(&self, key: Key) -> bool {
        self.keys_pressed.contains(key)
    }

    /// True on the frame `key` transitioned down -> up.
    pub fn is_key_released(&self, key: Key) -> bool {
        self.keys_released.contains(key)
    }

    /// Whether `button` is currently held.
    pub fn is_mouse_button_down(&self, button: MouseButton) -> bool {
        self.mouse_buttons.contains(button)
    }

    /// True on the frame `button` transitioned up -> down.
    pub fn is_mouse_button_pressed(&self, button: MouseButton) -> bool {
        self.mouse_buttons_pressed.contains(button)
    }

    /// True on the frame `button` transitioned down -> up.
    pub fn is_mouse_button_released(&self, button: MouseButton) -> bool {
        self.mouse_buttons_released.contains(button)
    }

    /// Reset the per-frame transient fields. Call at the start of each frame
    /// before folding in new events. Held state (`mouse_pos`, `mouse_buttons`,
    /// `keys_down`, `window_close_requested`) is preserved.
    pub fn begin_frame(&mut self) {
        self.mouse_motion = Vec2f::new(0.0, 0.0);
        self.mouse_wheel_delta = 0.0;
        self.mouse_buttons_pressed = MouseButtons::empty();
        self.mouse_buttons_released = MouseButtons::empty();
        self.keys_pressed = KeySet::empty();
        self.keys_released = KeySet::empty();
        self.text_input.clear();
        self.backspace = false;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mouse_buttons_insert_remove_contains() {
        let mut buttons = MouseButtons::empty();
        assert!(buttons.is_empty());
        assert!(!buttons.contains(MouseButton::Left));

        buttons.insert(MouseButton::Left);
        buttons.insert(MouseButton::Middle);
        assert!(buttons.contains(MouseButton::Left));
        assert!(buttons.contains(MouseButton::Middle));
        assert!(!buttons.contains(MouseButton::Right));

        buttons.remove(MouseButton::Left);
        assert!(!buttons.contains(MouseButton::Left));
        assert!(buttons.contains(MouseButton::Middle));

        let collected: Vec<_> = buttons.iter().collect();
        assert_eq!(collected, vec![MouseButton::Middle]);
    }

    #[test]
    fn mouse_buttons_bitwise_ops() {
        let mut a = MouseButtons::empty();
        a.insert(MouseButton::Left);
        let mut b = MouseButtons::empty();
        b.insert(MouseButton::Right);

        let union = a | b;
        assert!(union.contains(MouseButton::Left));
        assert!(union.contains(MouseButton::Right));

        let intersection = a & b;
        assert!(intersection.is_empty());
    }

    #[test]
    fn keyset_insert_remove_contains() {
        let mut keys = KeySet::empty();
        assert!(keys.is_empty());
        assert!(!keys.contains(Key::W));

        assert!(keys.insert(Key::W));
        assert!(!keys.insert(Key::W)); // already inserted
        assert!(keys.contains(Key::W));
        assert!(!keys.contains(Key::A));

        keys.insert(Key::F8);
        assert!(keys.contains(Key::F8));
        assert!(keys.contains(Key::W));

        assert!(keys.remove(Key::W));
        assert!(!keys.remove(Key::W));
        assert!(!keys.contains(Key::W));
        assert!(keys.contains(Key::F8));

        keys.clear();
        assert!(keys.is_empty());
    }

    /// Drives a synthetic two-frame sequence to verify transition tracking.
    #[test]
    fn key_transitions_two_frame_sequence() {
        let mut state = InputState::new();

        // Frame 1: W goes down.
        state.begin_frame();
        state.keys_down.insert(Key::W);
        state.keys_pressed.insert(Key::W);
        assert!(state.is_key_down(Key::W));
        assert!(state.is_key_pressed(Key::W));
        assert!(!state.is_key_released(Key::W));

        // Frame 2: W is held (still down, not pressed this frame).
        state.begin_frame();
        // keys_down preserved; keys_pressed cleared.
        assert!(state.is_key_down(Key::W));
        assert!(!state.is_key_pressed(Key::W));
        assert!(!state.is_key_released(Key::W));

        // Frame 3: W goes up.
        state.begin_frame();
        state.keys_down.remove(Key::W);
        state.keys_released.insert(Key::W);
        assert!(!state.is_key_down(Key::W));
        assert!(!state.is_key_pressed(Key::W));
        assert!(state.is_key_released(Key::W));
    }

    #[test]
    fn is_key_pressed_only_one_frame() {
        let mut state = InputState::new();

        state.begin_frame();
        state.keys_down.insert(Key::E);
        state.keys_pressed.insert(Key::E);
        assert!(state.is_key_pressed(Key::E));

        // Next frame: still down, but the press edge is gone.
        state.begin_frame();
        assert!(state.is_key_down(Key::E));
        assert!(!state.is_key_pressed(Key::E));
    }

    #[test]
    fn begin_frame_clears_only_transient_fields() {
        let mut state = InputState::new();

        // Populate every field.
        state.mouse_pos = Vec2f::new(100.0, 200.0);
        state.mouse_motion = Vec2f::new(5.0, -3.0);
        state.mouse_wheel_delta = 1.5;
        state.mouse_buttons.insert(MouseButton::Left);
        state.mouse_buttons_pressed.insert(MouseButton::Left);
        state.mouse_buttons_released.insert(MouseButton::Right);
        state.keys_down.insert(Key::W);
        state.keys_pressed.insert(Key::W);
        state.keys_released.insert(Key::A);
        state.text_input.push_str("abc");
        state.backspace = true;
        state.window_close_requested = true;

        state.begin_frame();

        // Preserved (held / sticky state).
        assert_eq!(state.mouse_pos, Vec2f::new(100.0, 200.0));
        assert!(state.mouse_buttons.contains(MouseButton::Left));
        assert!(state.keys_down.contains(Key::W));
        assert!(state.window_close_requested);

        // Cleared (per-frame transients).
        assert_eq!(state.mouse_motion, Vec2f::new(0.0, 0.0));
        assert!(state.mouse_wheel_delta.abs() < f32::EPSILON);
        assert!(state.mouse_buttons_pressed.is_empty());
        assert!(state.mouse_buttons_released.is_empty());
        assert!(state.keys_pressed.is_empty());
        assert!(state.keys_released.is_empty());
        assert!(state.text_input.is_empty());
        assert!(!state.backspace);
    }

    #[test]
    fn mouse_button_helpers() {
        let mut state = InputState::new();
        state.mouse_buttons.insert(MouseButton::Left);
        state.mouse_buttons_pressed.insert(MouseButton::Left);
        state.mouse_buttons_released.insert(MouseButton::Right);

        assert!(state.is_mouse_button_down(MouseButton::Left));
        assert!(state.is_mouse_button_pressed(MouseButton::Left));
        assert!(!state.is_mouse_button_released(MouseButton::Left));

        assert!(!state.is_mouse_button_down(MouseButton::Right));
        assert!(!state.is_mouse_button_pressed(MouseButton::Right));
        assert!(state.is_mouse_button_released(MouseButton::Right));
    }
}
