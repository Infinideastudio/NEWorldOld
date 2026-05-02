//! Per-cell block payload.
//!
//! `BlockData::default()` is the all-zero cell: `BlockId::EMPTY`,
//! `BlockState(0)` (inline zero), `BlockLight::NONE`. Chunk arrays are
//! initialised by `[BlockData::default(); SIZE_CUBED]`.

use serde::{Deserialize, Serialize};

use super::{BlockId, BlockLight, BlockState};

/// Per-cell payload stored in chunks. Three fields, encoded on disk as
/// `id (u16 LE) + state (u16 LE) + light (u8)` (5 bytes per cell).
#[derive(Copy, Clone, Debug, Eq, PartialEq, Default, Serialize, Deserialize)]
pub struct BlockData {
    pub id: BlockId,
    pub state: BlockState,
    pub light: BlockLight,
}

impl BlockData {
    /// Per-cell on-disk size (5 bytes: id u16 + state u16 + light u8).
    pub const ENCODED_LEN: usize = 5;

    /// Encode to little-endian bytes appended to `out`. Cheap loop: the
    /// chunk save path calls this once per cell.
    pub fn encode_to(self, out: &mut Vec<u8>) {
        out.extend_from_slice(&self.id.0.to_le_bytes());
        out.extend_from_slice(&self.state.0.to_le_bytes());
        out.push(self.light.0);
    }

    /// Decode from a 5-byte slice. Caller is responsible for splitting the
    /// chunk body into [`Self::ENCODED_LEN`]-sized chunks.
    #[must_use]
    pub fn decode_from(bytes: &[u8]) -> Self {
        debug_assert_eq!(bytes.len(), Self::ENCODED_LEN);
        Self {
            id: BlockId(u16::from_le_bytes([bytes[0], bytes[1]])),
            state: BlockState(u16::from_le_bytes([bytes[2], bytes[3]])),
            light: BlockLight(bytes[4]),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_is_all_zero() {
        let d = BlockData::default();
        assert_eq!(d.id, BlockId::EMPTY);
        assert_eq!(d.state, BlockState(0));
        assert_eq!(d.light, BlockLight::NONE);
    }

    #[test]
    fn encode_decode_round_trips() {
        let cases = [
            BlockData::default(),
            BlockData {
                id: BlockId(7),
                state: BlockState::inline(0x1234),
                light: BlockLight::new(15, 7),
            },
            BlockData {
                id: BlockId(0xFFFF),
                state: BlockState::external(0x7FFF),
                light: BlockLight::new(0, 0),
            },
        ];
        let mut buf = Vec::new();
        for &c in &cases {
            buf.clear();
            c.encode_to(&mut buf);
            assert_eq!(buf.len(), BlockData::ENCODED_LEN);
            assert_eq!(BlockData::decode_from(&buf), c);
        }
    }
}
