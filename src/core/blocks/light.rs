//! Packed sky/block light byte.

use serde::{Deserialize, Serialize};

/// Packed lighting byte: sky in the upper nibble, block in the lower nibble.
/// Each channel is in `0..=15`.
#[repr(C)]
#[derive(
    Copy, Clone, Debug, Eq, PartialEq, Hash, Ord, PartialOrd, Default, Serialize, Deserialize,
)]
pub struct BlockLight(pub u8);

impl BlockLight {
    /// Sky-only full daylight (sky=15, block=0).
    pub const SKY: BlockLight = BlockLight::new(15, 0);
    /// Total darkness.
    pub const NONE: BlockLight = BlockLight::new(0, 0);

    /// Pack `(sky, block)` nibbles. Both must be `<= 15`.
    #[must_use]
    pub const fn new(sky: u8, block: u8) -> Self {
        assert!(sky <= 15, "BlockLight::new: sky > 15");
        assert!(block <= 15, "BlockLight::new: block > 15");
        Self((sky << 4) | (block & 0x0F))
    }

    #[must_use]
    pub const fn sky(self) -> u8 {
        self.0 >> 4
    }

    #[must_use]
    pub const fn block(self) -> u8 {
        self.0 & 0x0F
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trips_sky_block() {
        let l = BlockLight::new(15, 0);
        assert_eq!(l.sky(), 15);
        assert_eq!(l.block(), 0);
        let l2 = BlockLight::new(7, 11);
        assert_eq!(l2.sky(), 7);
        assert_eq!(l2.block(), 11);
        assert_eq!(BlockLight::SKY, BlockLight::new(15, 0));
        assert_eq!(BlockLight::NONE, BlockLight::new(0, 0));
    }
}
