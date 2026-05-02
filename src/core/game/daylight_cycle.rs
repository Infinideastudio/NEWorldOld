//! `DaylightCycle` — the in-game clock.
//!
//! A monotonically-advancing tick counter, wrapping at one in-game
//! day. Drives sun direction, sky-light, and any future time-of-day
//! consumers. Lives outside `World` because the database doesn't care
//! about wall-clock time; it's pure gameplay state.

/// One in-game day in sim ticks. 30 ticks/sec × 60 s × 20 min = 36000.
/// Sun-direction and sky-light multipliers wrap on this period.
pub const DAY_TICKS: u32 = 30 * 60 * 20;

#[derive(Clone, Copy, Debug, Default)]
pub struct DaylightCycle {
    game_time: u32,
}

impl DaylightCycle {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn game_time(&self) -> u32 {
        self.game_time
    }

    pub fn set_game_time(&mut self, t: u32) {
        self.game_time = t;
    }

    /// Advance the in-game clock by `ticks`. Wraps at [`DAY_TICKS`] so
    /// the clock remains comparable across a long-running world.
    pub fn advance(&mut self, ticks: u32) {
        self.game_time = self.game_time.wrapping_add(ticks) % DAY_TICKS;
    }
}
