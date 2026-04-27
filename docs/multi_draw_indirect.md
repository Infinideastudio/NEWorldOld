# Multi-Draw-Indirect Chunk Rendering — Design

## Motivation

`samply` profiling at `render_distance = 32` with a fully-meshed neighborhood
attributes ~43% of frame CPU time to `wgpu::CommandEncoder::finish`. The cost
scales with the number of recorded draw commands. Per frame, the world pass
records:

| Pass                          | Mode      | Draws per chunk |
|-------------------------------|-----------|-----------------|
| Shadow opaque                 | advanced  | 1               |
| G-buffer opaque               | advanced  | 1               |
| Translucent G-buffer          | advanced  | 1               |
| Forward opaque                | basic     | 1               |
| Forward translucent           | both      | 1               |

At ~5 K visible chunks × 4–5 passes that's 20–25 K draws per frame, each
recording one `set_vertex_buffer` + one `draw`. After frustum culling
(see `Game::camera_frustum`), GPU utilization climbs to ~55% and CPU stays
pinned on encoder work — recorded-command count is the bottleneck.

The fix is structural: replace per-chunk vertex buffers and per-chunk draws
with a single arena buffer per layer plus `multi_draw_indirect`, where
contiguous runs of visible chunks fuse into single draws.

## Goals

1. **One `multi_draw_indirect` call per render pass** — encoder cost
   independent of visible chunk count.
2. **Few indirect-list entries per pass** — GPU dispatch cost stays low
   even at high render distance.
3. **No mid-frame compaction stalls** — fragmentation must not produce
   visible hitches.
4. **No new GPU memory hot path** — uploads stay on `queue.write_buffer`
   plus the existing meshing worker.

## Architecture overview

Two persistent vertex arenas (one for opaque chunk geometry, one for
translucent), each backed by a single `wgpu::Buffer`. Every `ChunkMesh`
holds an `ArenaSlot` per layer — a `(offset, bucket, vertex_count)`
triple — instead of an owned buffer. Per pass, per frame:

1. Cull the visible set (already done — `Game::camera_frustum` +
   distance cube).
2. Sort visible slots by offset.
3. Walk in offset order, fusing runs separated by ≤ `MAX_GAP`
   non-visible vertices.
4. Emit one `DrawIndirectArgs` per fused run into a persistent indirect
   buffer.
5. `set_vertex_buffer(arena.buffer)` once, then `multi_draw_indirect`.

The fragmentation problem that arises from sub-allocating the arena is
neutralized by the **zero-on-free invariant** (see §Allocator below):
freed slots are filled with degenerate triangles, which the rasterizer
rejects in setup at no fragment cost. Drawing across a freed range
costs only the vertex shader work for the zeroed verts — same per-vert
cost as drawing a frustum-culled live chunk that gets clipped post-VS.

This unlocks the merge step: any "non-visible" interior between two
visible chunks (free hole, internal-waste tail, frustum-culled chunk)
can be drawn through, paying VS time but no fragment time. The
`MAX_GAP` knob trades that VS time against `multi_draw_indirect` entry
count.

## Allocator — bucketed segregated free lists

### Buckets

Power-of-two vertex counts:

```
BUCKET_SIZES: [256, 512, 1024, 2048, 4096, 8192, 16_384, 32_768]
```

Eight buckets covers the realistic chunk vertex-count range (sky chunks
~600 verts, surface ~3 K, dense forest/cave ~8 K, pathological ≤ 16 K).
A request larger than 32 K is a hard error that triggers buffer growth.

Power-of-two spacing yields ≤ 2× internal waste per slot, ~25%
average. A finer geometric sequence (e.g. 1.5×) reduces this to ~12%
at the cost of more buckets — defer until profiling justifies it.

### Per-arena state

```rust
struct ChunkArena {
    buffer: wgpu::Buffer,                 // capacity_verts * vertex_stride bytes
    capacity_verts: u32,
    high_water_verts: u32,                // first never-allocated vertex
    free_lists: [Vec<u32>; N_BUCKETS],    // each entry is a vertex offset
}

struct ArenaSlot {
    offset: u32,         // first_vertex (in vertices, not bytes)
    bucket: u8,          // index into BUCKET_SIZES — needed for free()
    vertex_count: u32,   // actual count used (≤ BUCKET_SIZES[bucket])
}
```

`free_lists[i]` stores offsets only — capacity is implicit from the
bucket index.

### Allocate

```text
fn allocate(verts) -> Result<ArenaSlot, ArenaFull>:
    bucket = pick_bucket(verts)
    cap    = BUCKET_SIZES[bucket]

    if let Some(off) = free_lists[bucket].pop():
        return ArenaSlot { offset: off, bucket, vertex_count: verts }

    if high_water_verts + cap > capacity_verts:
        return Err(ArenaFull)         // caller grows the arena

    off = high_water_verts
    high_water_verts += cap
    return ArenaSlot { offset: off, bucket, vertex_count: verts }
```

O(1). The caller follows up with one `queue.write_buffer` covering the
prefix `[offset, offset + verts)`. The trailing `cap - verts` of the
slot stays zero — either inherited from the last `clear_buffer` on this
offset, or from initial buffer zero-fill on first carve.

### Free

```text
fn free(slot):
    encoder.clear_buffer(buffer,
                         slot.offset * stride,
                         BUCKET_SIZES[slot.bucket] * stride)
    free_lists[slot.bucket].push(slot.offset)
```

`clear_buffer` zeroes the **whole bucket capacity**, not just
`vertex_count`. This reestablishes the invariant: the entire slot range
holds degenerate (all-zero-position) triangles. The rasterizer's
triangle-setup phase rejects them at zero fragment cost; only the VS
runs per vertex.

The clear is recorded into the next frame's encoder, batched with all
other frees from the same frame. wgpu schedules it on the COPY queue,
overlapping the GFX queue's render work.

### Realloc fast path

When `pump_meshing` rebuilds a chunk's mesh, the new vertex count
usually stays in the same bucket. Skip the free/alloc round-trip:

```text
fn realloc(old: ArenaSlot, new_verts) -> Result<ArenaSlot, ArenaFull>:
    if pick_bucket(new_verts) == old.bucket:
        return ArenaSlot { vertex_count: new_verts, ..old }
    free(old)
    allocate(new_verts)
```

If the new mesh is shorter than the old one, the caller writes the
shorter prefix and **must zero the suffix** `[offset + new_verts, offset
+ old_verts)` — those vertices were live data, not degenerates.
Cleanest: queue an extra `clear_buffer` for the suffix range before the
new `write_buffer`.

### Buffer growth

When `allocate` returns `ArenaFull`:

1. Create a 2× capacity buffer.
2. `encoder.copy_buffer_to_buffer(old, 0, new, 0, high_water * stride)`.
3. Replace `arena.buffer` with the new one. **Slots' offsets remain
   valid** — the linear copy preserves them.
4. The freshly-grown tail is automatically zero (wgpu default).
5. Drop the old buffer at end of frame (after the copy submits).

One frame stall per growth event. With sensible initial sizing (see
§Sizing) this is rare.

## Per-frame indirect-list build

```rust
// Visible slots, sorted by offset.
let mut runs: Vec<(u32, u32)> = visible.iter()
    .filter_map(|cm| cm.opaque_slot.as_ref().map(|s| (s.offset, s.vertex_count)))
    .collect();
runs.sort_unstable_by_key(|&(off, _)| off);

// Fuse runs separated by ≤ MAX_GAP non-visible verts.
let mut args: Vec<DrawIndirectArgs> = Vec::with_capacity(runs.len());
let mut cur_start = 0u32;
let mut cur_end   = 0u32;       // exclusive
for &(off, count) in &runs {
    if cur_end == 0 {
        cur_start = off;
        cur_end   = off + count;
        continue;
    }
    let gap = off.saturating_sub(cur_end);
    if gap <= MAX_GAP {
        cur_end = off + count;  // absorb the gap into the run
    } else {
        args.push(DrawIndirectArgs {
            vertex_count:   cur_end - cur_start,
            instance_count: 1,
            first_vertex:   cur_start,
            first_instance: 0,
        });
        cur_start = off;
        cur_end   = off + count;
    }
}
if cur_end != 0 {
    args.push(DrawIndirectArgs {
        vertex_count:   cur_end - cur_start,
        instance_count: 1,
        first_vertex:   cur_start,
        first_instance: 0,
    });
}

queue.write_buffer(indirect_buf, 0, bytemuck::cast_slice(&args));
```

O(N log N) sort + O(N) merge, where N = visible chunks per pass. At
N=5 K, sort ~80 µs, merge ~20 µs — both dwarfed by the encoder cost
they replace.

### `MAX_GAP` knob

Each closed-and-reopened draw range costs:
- ~50–200 ns GPU dispatch overhead.
- ~50–100 ns CPU encoder finish work for the indirect entry.
- Total ~200–400 ns per saved indirect entry.

Each extra vertex drawn (degenerate or culled-clipped) costs:
- 1 / VS_throughput ≈ 0.5 ns at 2 G verts/s (realistic for `chunk.wgsl`).

Break-even is at ~600 vertices per saved entry. **Recommended starting
value: `MAX_GAP = 1024`**, biasing toward merging in the marginal
window. Tune from telemetry: track `args.len() / visible.len()` and
`drawn_verts / live_visible_verts`; dial `MAX_GAP` up if the first
ratio is high, down if the second is high.

## Cost model

| Operation                              | Cost                                      |
|----------------------------------------|-------------------------------------------|
| `allocate(verts)`                      | O(1) — Vec::pop or bump                   |
| `free(slot)`                           | O(1) — Vec::push, COPY-queue clear        |
| `realloc(slot, new_verts)`             | O(1) same-bucket / O(1) free + allocate   |
| Per-pass indirect build                | O(N log N) sort + O(N) merge              |
| Per-pass GPU draw cost                 | O(visible_verts) VS + fragment work       |
| Per-pass GPU dispatch cost             | O(args.len()) — small with MAX_GAP > 0    |
| Buffer growth                          | O(high_water) GPU copy, rare              |
| Compaction (optional, see §below)      | O(live_verts) GPU copy, rare              |

`CommandEncoder::finish` cost goes from O(visible_chunks) to
O(args.len()), reducing it by 1–2 orders of magnitude with a
spatially-coherent allocator and `MAX_GAP > 0`.

## Spatial locality and optional compaction

The merge ratio depends on visible chunks being buffer-adjacent.

- **Random allocator order:** average run length ~1.3, merging saves
  ~25%. Useful but lukewarm.
- **Spatially-coherent order:** visible chunks form a few large runs,
  merging cuts entry count 20–100×.

Initial allocations follow `tick_chunk_loading_async`'s spiral-from-
center order naturally, so spatial locality is good on cold load. Long
sessions degrade this as chunks free + reallocate from buckets'
free lists (LIFO order, no spatial bias).

**Optional compaction** restores locality when needed:

- Trigger metric: `drawn_verts / live_visible_verts` or
  `high_water_verts / sum(BUCKET_SIZES[slot.bucket] for slot in live)`.
  If either climbs above ~1.3, schedule compaction.
- Algorithm: walk live slots in **Morton-key order on chunk coord**,
  copy each to a fresh arena via `copy_buffer_to_buffer`, swap.
- Incremental: copy a slice per frame over ~10 frames; both old and
  new arenas hold valid data during the transition (degenerate
  tails / freed slots are zero in both), so reads are safe either way.
  Slot offsets get updated atomically when each chunk's slot is rewritten.

Compaction is **never required for correctness** under the zero-on-free
invariant — fragmentation only inflates VS cost, never produces
artifacts. Skip implementing compaction in v1; revisit if telemetry
shows it pays off.

## Sizing

Initial arena capacity (per layer): pick the larger of

- `2 × peak_meshed_chunks × mean_chunk_verts × stride`, or
- 256 MB for opaque, 64 MB for translucent (typical voxel split).

At rd=32 with ~10 K meshed non-empty chunks × ~3 K average verts ×
36 B = ~1.1 GB, so 256 MB initial + 2× growth twice lands us at 1 GB.
Fine for any modern dGPU; tight on integrated. Worth a runtime check
of `wgpu::Limits::max_buffer_size` and a graceful "render distance
must be ≤ N at this hardware level" message on init.

Indirect buffer per pass: `MAX_VISIBLE_CHUNKS × 16 B`, persistent,
grow-on-need. At 5 K visible × 16 B = 80 KB. Negligible.

## wgpu features

**Required:** `wgpu::Features::MULTI_DRAW_INDIRECT`. Available on
D3D12, Vulkan, and Metal Tier 2. Adapter request must include it; if
the chosen adapter doesn't support it, fall back to a different
adapter or fail init with a clear message.

**Not used:**
- `MULTI_DRAW_INDIRECT_COUNT` — would let the GPU decide draw count.
  Useful for v3 GPU-side culling only.
- `INDIRECT_FIRST_INSTANCE` — we always pass `first_instance = 0`.
- `Float32Blendable` — already not used (translucent G-buffer is
  `Rgba16Float`).

**No software fallback path.** A loop of per-chunk `draw_indirect`
calls would defeat the purpose; if the design needs to ship on
hardware without `MULTI_DRAW_INDIRECT`, it's the wrong design for
that hardware tier and should fall back to the current per-chunk
pipeline.

## Layered separation

Opaque and translucent each get their own arena. Reasons:

- Different VS / FS work per layer; mixing them in one arena would
  conflate VS-cost telemetry.
- Translucent chunks are rarer and smaller; a 64 MB arena suffices
  vs 256 MB for opaque, saving GPU memory.
- Free-list sizes diverge significantly per layer; one set of buckets
  per arena keeps the free lists cache-warm.

The shadow pass uses the **opaque arena only** (translucent doesn't
shadow — water/leaves match C++ behavior). Same merge logic, separate
visible set (`shadow_visible`), separate indirect buffer.

## Migration phases

The migration can land incrementally. Each phase compiles and runs;
each one delivers measurable wins.

### Phase 1: arena allocator + per-chunk MDI

- Replace per-chunk `wgpu::Buffer` with `(opaque_slot, translucent_slot)
  : (Option<ArenaSlot>, Option<ArenaSlot>)`.
- Implement bucketed allocator with zero-on-free.
- Build per-pass indirect buffer with **one entry per visible chunk**
  (no merging). `MAX_GAP = 0`.
- Use `multi_draw_indirect`.

This validates the allocator and the indirect-pipeline plumbing. CPU
encoder cost should drop substantially even without merging
(`set_vertex_buffer` happens once, not per chunk; `draw` calls collapse
to a single `multi_draw_indirect`).

### Phase 2: range merging with `MAX_GAP`

- Add the sort + merge step.
- Plumb `MAX_GAP` through config (default 1024).
- Add F3 telemetry: `args.len()`, `drawn_verts`, `live_visible_verts`.

This delivers the encoder-cost reduction.

### Phase 3 (optional): compaction

- Add compaction trigger metric.
- Implement incremental Morton-sort compaction.

Only worth doing if Phase 2 telemetry shows merge ratio degrading
during long sessions.

### Phase 4 (optional): GPU-side culling

- Replace CPU frustum cull + sort + merge with a compute shader that
  reads all slots and writes the indirect buffer + count.
- Use `MULTI_DRAW_INDIRECT_COUNT`.

Only worth doing if Phase 2/3 telemetry shows the CPU sort + merge has
become the bottleneck (unlikely at our scale).

## Out of scope

Designs that were considered and rejected:

- **Indexed drawing with paged vertex storage.** Eliminates internal
  fragmentation but needs an index buffer (~11% memory overhead) plus
  an allocator for index storage that faces the original fragmentation
  problem. Strictly worse than bucketed + zero-on-free.
- **Page allocator with multiple draws per chunk.** Multiplies indirect
  entry count by K = pages-per-chunk. Defeats the encoder-cost goal.
- **Coalescing free list (dlmalloc-style).** Recovers external
  fragmentation incrementally, but the zero-on-free invariant makes
  cross-bucket fragmentation cheap, removing the motivation. Bucketed
  is simpler and equivalent in practice.
- **Compute-shader scatter for uploads.** Useful at very high upload
  rates with many pages per chunk; at our scale (≤ 50 chunks meshed
  per frame, K ≈ 1) `queue.write_buffer` is sufficient.
- **Transient per-frame ring buffers.** Bandwidth analysis shows
  re-uploading all visible chunks every frame would be ~500 MB/s at
  60 FPS — workable but wasteful. Persistent arenas are strictly
  better.
- **Single combined arena (opaque + translucent).** Maximizes packing
  density slightly; complicates per-layer telemetry and growth. Not
  worth the coupling.

## Risks and mitigations

| Risk                                              | Mitigation                                       |
|---------------------------------------------------|--------------------------------------------------|
| Spatial locality silently degrades over long play | F3 telemetry on merge ratio; compaction in Ph. 3 |
| Adapter lacks `MULTI_DRAW_INDIRECT`               | Detect at init; fail with explicit error         |
| Buffer growth stall on long-distance teleport     | Pre-size 256 MB; 2× growth absorbs the spike     |
| `realloc` suffix-zero cost on shrink              | Single `clear_buffer` per shrink, COPY queue     |
| Vertex-shader cost on degenerates accumulates     | Compaction (Ph. 3); trigger on drawn-VS metric   |

## References

- Current per-chunk pipeline: `src/worlds/chunk_rendering.rs`,
  `src/game/mod.rs::record_world_pass`.
- Frustum culling already in place: `Game::camera_frustum`,
  `record_world_pass` (post-`a5e3690`).
- `wgpu` MDI docs:
  https://docs.rs/wgpu/latest/wgpu/struct.RenderPass.html#method.multi_draw_indirect
- C++ predecessor's draw dispatch (no MDI):
  `old/src/worlds/world_rendering.cpp`.
