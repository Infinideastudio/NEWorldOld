# Rust Migration Plan

This document captures a module-by-module plan for porting NEWorld from C++23 modules
to Rust. It identifies which parts of the existing C++ code are "old" (carry-over from
the pre-modernization rewrite, with global state and weak abstractions) and need a clean
re-design, and which parts already fit the C++ structure that the modernization rewrite
established and can be ported as-is.

The plan also fixes ownership relations up front, so the borrow checker has a static
story that matches the existing data flow.

### No backward compatibility

The Rust port is a clean break. There is **no requirement to read existing C++-built
worlds, player saves, options files, or language files**, and **no requirement to
preserve any on-disk byte layout or chunk DB key encoding**. This means we are free to:

* Pick whatever block / chunk / player serialization format is most ergonomic in Rust
  (`bincode`, `postcard`, `rkyv`, or hand-rolled — no need to match `reinterpret_cast`
  semantics).
* Use whatever chunk DB key shape we like (typed struct, varint-packed coords, plain
  `[i32; 3]` little-endian — no need to keep the old 64-bit packed `ChunkId` shape;
  the in-memory `ChunkKey` is a slotmap generation key, separate from anything stored
  on disk).
* Switch options storage from INI to TOML.
* Re-author the language tables (`lang/keys.lk` + `lang/*.lang`) in any format we
  prefer (e.g. one TOML/JSON per language).

We may still keep the **on-disk directory layout** (`worlds/<name>/{chunks.db,
player.<ext>, thumbnail.png}`) and the world-name-as-folder convention because they
are user-visible, but the file *contents* are open for redesign.

---

## 1. Inventory of the existing C++ code

### 1.1 Old code (carry-over, must be re-designed)

These files keep ~all program-wide mutable state in module-level globals, share data
across translation units by name, and were called out by the user as still in their
pre-modernization form.

| File | What it is | Why it is "old" |
|------|------------|-----------------|
| `globals.ixx` | ~50 free `export` mutable variables: `WindowWidth`, `mx/my/mw/mb`, `inputstr`, `RenderDistance`, `AdvancedRender`, `GameTime`, `MainWindow`, `g_seed` (RNG), counters | Pure global-variable bag. Every module reads/writes these freely. |
| `setup.ixx` | GLFW init, window size/mouse/scroll/key callbacks, fullscreen toggle, texture loading | Callbacks write straight to `globals.ixx` mutables. `splash_screen()` mixes init flow with rendering. |
| `textures.ixx` | A bag of `export render::Texture` globals, `TextureIndex` enum, hardcoded `getTextureIndex(blocks::Id, face)` table | Atlas indices live in a `constexpr std::array<std::array<…,3>,…>` keyed by block id and face — duplicates the block registry. Globals leak through every render call. |
| `text_rendering.ixx` | FreeType handle, atlas image, atlas texture, atlas cursor (`curr_row/col`), font color — all module-level mutables | Stateful but never owned by anything; reload requires manually clearing several global fields. |
| `terrain_generation.ixx` | `_perm`, `_seed` module-level mutables; uses global `fast_srand`/`rnd` | Generator carries hidden state; not seedable per world without touching globals. |
| `rendering.ixx` (the `Renderer` namespace) | `std::vector<render::Program> shaders`, `std::vector<render::Texture> textures`, `std::vector<render::Framebuffer> framebuffers`, `sunlightHeading` etc. | Holds the entire GPU pipeline as a singleton. Indices into these vectors are exported as enums (`UIShader`, `OpaqueShader`, …). |
| `particles.ixx` | `std::vector<Particle> ptcs`, `pxpos/pypos/pzpos` module-level mutables | Update/mesh/render are free functions over module state. |
| `globalization.ixx` | `std::map<int, Line> Lines`, `std::map<std::string,int> keys` as module mutables | Singleton i18n table. |
| `neworld.ixx` (1369 lines) | The "god file": `main()`, the update thread, input dispatch, block-pick raycast, HUD drawing, inventory drawing, screenshot/thumbnail capture, breaking-overlay drawing | Mixes orchestration, simulation, input, and rendering. State (`selx/sely/selz`, `seldes`, `bagOpened`, `chatmode`, `chatword`, `chat_messages`, `keyDown[]`, `update_mutex`, `update_timer`, fps/ups counters) is at file scope. |

### 1.2 New code (modernization rewrite, mostly portable as-is)

| File | What it is | Notes |
|------|------------|-------|
| `types.ixx` | `int8_t…uint64_t`, `FixedString<N>` for non-type template args | Drops out entirely in Rust — `glam` handles vector typedefs and Rust has no fixed-string NTTP need. |
| `debug.ixx` | `assert`, `unreachable`, `unimplemented` | Replace with `assert!`, `unreachable!()`, `todo!()`. |
| `math/{vector,matrix,euler,aabb,frustum}.ixx` | Templated Vec/Mat/AABB/Frustum/Euler | Replace with `glam` + thin wrappers for `Aabb3`, `Frustum`, `Euler`. |
| `render/*` | OpenGL RAII wrappers (`Buffer`, `Texture`, `Framebuffer`, `Program`, `VertexArray`); compile-time interface-block descriptors (`block_layout`, `attrib_layout`); image I/O | Replace **wholesale** with `wgpu`. The compile-time layout magic stops being useful when `wgpu` already type-checks bind groups/vertex layouts at runtime against shader reflection. |
| `ui/{context,element,layout,render,controls/*}.ixx` | A Flutter-style declarative UI library (View/Element/Builder, Row/Column/Stack, Sizer/Padding, Button/Slider/TextBox/ImageBox/ScrollView) | Replace with `egui` — it covers exactly this surface area and is integrated with `winit`/`wgpu`. |
| `blocks.ixx` | `Id`, `State`, `Light`, `BlockData`, `BlockInfo`, `BlockInfoRegistry`, `BaseBlocks` | Solid design. Port directly. The two trailing `export` globals (`block_info_registry`, `base_blocks`) are explicitly tagged "Temporary: compatibility interface with the old code" and should be deleted in the port. |
| `chunks.ixx` | `Chunk` with lazy `unique_ptr<array<…>>` data, terrain generation entry point, package/unpackage | Solid design. Port directly. |
| `chunk_pointer_arrays.ixx` | A 3D sliding-window cache of `chunks::Chunk*` keyed by world coord — its purpose is to bypass the chunk hashmap on hot paths | Keep as an O(1) coord-keyed cache; replace the raw pointer with a `slotmap::ChunkKey` (8 bytes). See §2.1 — storing `ChunkId` would defeat the cache. |
| `height_maps.ixx` | 2D sliding-window cache of terrain heights | Port directly. |
| `worlds/worlds.ixx` | `World`: chunk hashmap + parallel `_renders` map, `TilesStore` (LevelDB), block-update queue, chunk load/unload/meshing pipelines, render-chunk listing | Solid orchestration; ownership rewires (see §2). The chunk hashmap, `_renders` map, `_chunk_pointer_cache_value`, and `RenderData::_refer` collapse into a single `SlotMap<ChunkKey, ChunkSlot { chunk, render }>`. The transient `_chunk_meshing_list<RenderData*>` becomes `Vec<(i32, ChunkKey)>`. |
| `worlds/player.ixx` | `Player` with coord/velocity/orientation, gamemode, inventory, save/load | Solid design. Save format uses raw `reinterpret_cast` — replace with `serde` + a versioned binary encoding (`bincode` or `postcard`). |
| `worlds/{chunk_rendering,world_rendering}.cpp` | Greedy-meshing routine for chunks; per-chunk draw call dispatch | Port logic directly into the new `wgpu`-based renderer. |
| `worlds/player_impl.cpp` | Player physics tick | Port directly. |
| `commands.ixx` | Chat command registry | Port directly. |
| `items.ixx` | `ItemStack` | Port directly. |
| `menus.ixx` + `menus/*.cpp` | Modal menus built on the `ui` library | Reimplement in `egui`; the structure is straightforward. |

### 1.3 Out of scope for the port

* `src/glad/*` — GL function loader, replaced by `wgpu`.
* `cmake/*`, `vcpkg.json`, `vcpkg-configuration.json` — replaced by Cargo.
* `shaders/*.{vsh,fsh}` — must be rewritten as WGSL (or kept as GLSL and cross-compiled
  via `naga`; we will rewrite to WGSL during the renderer rewrite — the GLSL is small,
  ~22 KB total, dominated by `final.fsh`).

---

## 2. Ownership and borrow plan

The C++ code uses raw `chunks::Chunk*` references in several places that don't survive
in Rust without `unsafe`. The port resolves this with a single rule:

> **The `World` is the unique owner of all chunk data.** Lookup goes through a
> generational arena indexed by an O(1) array; the sliding-window pointer cache stays.
> No long-lived `&Chunk` references into the arena are stored anywhere.

### 2.1 Why not `HashMap<ChunkId, Chunk>` + `ChunkPointerArray<ChunkId>`

The C++ design has two complementary lookup paths:

* `_chunks: unordered_map<ChunkId, unique_ptr<Chunk>>` — the canonical owner; bucketed
  hash, but indirect (one hash + one pointer chase per access).
* `ChunkPointerArray` — a 3D sliding window of raw `Chunk*` keyed directly by world
  coord; one integer index, one pointer chase. The whole point of this structure is to
  bypass the hashmap on the hot paths (block-update queue, mesh neighbor reads, player
  hitbox queries).

If we store `ChunkId` inside the array we still pay the hashmap cost per access — the
cache becomes a memory-wasting indirection. We need pointer-stability across map
operations, which Rust expresses with a generational arena rather than raw `&Chunk`.

### 2.2 Layout

Use [`slab::Slab`] as the canonical chunk store. `Slab` is the minimal primitive that
fits this problem: an arena keyed by `usize`, supporting `insert(value) -> usize`,
`remove(key) -> value`, `get(key) -> Option<&value>`, and contiguous iteration. It is
plain `Vec<Entry>` + freelist with no generation tag and no garbage collection.

This is the right level of abstraction here because **`World` is the sole, explicit
owner of every chunk's lifetime**: chunks are allocated only inside `World::load_chunk`
and freed only inside `World::unload_chunk`, both bracketed by atomic updates of the
`chunk_grid`. Slot recycling is therefore not a foreign event the chunk store has to
defend against — it is a controlled operation `World` performs on itself. The
generation-tag overhead a `slotmap::SlotMap` would add buys us nothing in this design;
we recover the same anti-aliasing discipline by funnelling every cross-frame /
cross-thread reference through a coord (see §2.5 on async meshing).

```rust
pub type ChunkKey = usize;          // a slab index, scoped to one World

pub struct ChunkSlot {
    pub chunk:  Chunk,
    pub render: ChunkRender,        // mesh, VBO, load anim — collocated, single lifetime
}

pub struct World {
    name: String,
    tiles_store: TilesStore,                       // sled DB handle
    chunks:     Slab<ChunkSlot>,                   // canonical owner
    by_coord:   HashMap<IVec3, ChunkKey>,          // every loaded chunk, grid or not
    chunk_grid: ChunkGrid,                         // sliding 3D hot-path cache
    height_map: HeightMap,
    loaded_core: LoadedCore,
    block_update_queue: VecDeque<IVec3>,
    load_list:    Vec<(i32, IVec3)>,
    unload_list:  Vec<(i32, IVec3)>,
    meshing_list: Vec<(i32, ChunkKey)>,
    player: Player,
    game_time: u32,
}
```

`ChunkGrid` is the renamed `ChunkPointerArray`: a `Vec<Option<ChunkKey>>` of size
`(2·(RenderDistance+2))³` plus an `origin: IVec3` and `size: usize`. `set_center`
shifts the array exactly like the C++ version.

The world keeps **two coord-keyed indices** with different roles:

* `chunk_grid` — the hot-path cache. O(1) array access, no hashing. Covers the
  sliding render-distance window only. This is what `update_block`, meshing's
  27-neighbor scan, particles, and player physics go through.
* `by_coord` — the canonical "is this coord loaded?" map. Covers **every loaded
  chunk**, regardless of whether it currently sits inside the grid window. This is
  the structural counterpart of the C++ `_chunks` hashmap. Cold paths
  (load/unload bookkeeping, save-all enumeration, future "anchored" chunks that
  stay loaded outside the render window) go through it.

The grid is therefore a *subset* view of `by_coord`: a chunk inside the grid window
appears in both; a chunk that is loaded but outside the window appears only in
`by_coord` (with the grid cell at its world coord either empty or unrelated). The
C++ build doesn't currently load chunks outside the render window, but keeping the
hashmap from day one means future anchor-loaded chunks don't force a structural
rewrite.

Iteration over every loaded chunk (`save_to_files`, the unload-list builder) goes
directly through `chunks.iter()` on the slab — contiguous and cache-friendly — and
uses `slot.chunk.coord()` for filtering. `by_coord` is consulted only when starting
from a coord and not knowing whether the chunk is in the grid window.

```
GameApp (root)
├── Config / I18n / BlockRegistry (Arc) / Atlases / InputState / Renderer / UiState
├── ParticleSystem      (ticks against &impl BlockView)
├── ChatState           (chatword, history, command registry)
├── HudState            (selx/sely/selz, seldes, bagOpened, FOVyExt)
└── World (Option<World>) — see above
```

### 2.3 Lookup paths

```rust
impl World {
    // Hot path: O(1) array index + O(1) slab get. No hashing.
    pub fn chunk(&self, ccoord: IVec3) -> Option<&Chunk> {
        let key = self.chunk_grid.get(ccoord)?;
        Some(&self.chunks[key].chunk)
    }
    pub fn chunk_mut(&mut self, ccoord: IVec3) -> Option<&mut Chunk> {
        let key = self.chunk_grid.get(ccoord)?;
        Some(&mut self.chunks[key].chunk)
    }

    // Cold path: covers every loaded chunk, including those outside the grid window.
    pub fn chunk_by_coord(&self, ccoord: IVec3) -> Option<&Chunk> {
        let key = *self.by_coord.get(&ccoord)?;
        Some(&self.chunks[key].chunk)
    }

    fn insert_chunk(&mut self, ccoord: IVec3, slot: ChunkSlot) -> ChunkKey {
        let key = self.chunks.insert(slot);
        self.by_coord.insert(ccoord, key);
        if self.chunk_grid.contains(ccoord) {
            self.chunk_grid.set(ccoord, Some(key));    // grid is the hot-path cache
        }
        key
    }

    fn remove_chunk(&mut self, ccoord: IVec3) -> Option<ChunkSlot> {
        let key = self.by_coord.remove(&ccoord)?;
        if self.chunk_grid.contains(ccoord) {
            self.chunk_grid.set(ccoord, None);         // clear cache *before* free
        }
        Some(self.chunks.remove(key))
    }
}
```

Insert / remove encapsulate the three-structure update (`chunks`, `by_coord`,
`chunk_grid`) so callers cannot leave them inconsistent. The `chunk_grid.set(_, None)`
clearing in `remove_chunk` mirrors the C++ `_chunk_pointer_cache_value = nullptr`
discipline — without it, a stale `ChunkKey` in the grid would alias a freshly-recycled
slot that now holds a different chunk. When the grid window slides past a chunk that
stays loaded (e.g. an anchored chunk), `set_center` clears just the grid cells, leaves
`by_coord` untouched, and the chunk transitions from "hot-path accessible" to
"`by_coord`-only".

### 2.4 Why this works for borrows

1. **`ChunkRender` is collocated with the chunk** in `ChunkSlot`. Mesh building is a
   function `build_mesh(input: &MeshInput) -> MeshOutput` over a *snapshot* of the
   27-neighborhood (read into an owned `MeshInput` struct on the main thread, then
   shipped to a worker via `crossbeam_channel`). The worker never holds a reference
   into the slab; the main thread writes the resulting `MeshOutput` back into
   `chunks[key].render` after the worker returns it. Async result re-resolution goes
   by **coord** (`MeshOutput { coord, … }`), not by stale key, so a slot recycled
   while the worker was running cannot be aliased — see §2.5.
2. **`Player` lives inside `World`.** Methods needing both `&mut Player` and `&World`
   borrows use field-disjoint destructuring:
   ```rust
   pub fn tick_player(&mut self) {
       let World { player, chunks, chunk_grid, .. } = self;
       let view = ChunkGridView { chunks, chunk_grid };  // impl BlockView
       player.tick(&view);
   }
   ```
   The `ChunkGridView<'a>` is a tiny `&'a Slab` + `&'a ChunkGrid` pair; it forwards
   `block(coord)` through the same fast path as `World::chunk`.
3. **`BlockView` trait** abstracts read-only block lookup. Implemented for
   `ChunkGridView<'_>` and for `ChunkNeighborhood<'a>` (the meshing snapshot).
   Particles, player physics, and HUD raycasts all consume `&impl BlockView` instead
   of `&mut World`.
4. **No global statics for game state.** `GameApp` is created in `main`, threaded
   through `winit`'s event loop (the `ApplicationHandler` in winit 0.30+), and
   dropped on exit. Renderer pipelines live on `GameApp.renderer`, not in a
   `Renderer::` namespace.
5. **Block registry is read-only after init.** Hold it as `Arc<BlockRegistry>` in
   `GameApp` and clone the `Arc` into worker threads. No interior mutability needed.
6. **Texture atlas indices live with the registry.** The `TextureIndex` enum and the
   per-block `[face0, face1, face2]` table fold into `BlockInfo`. Eliminates the
   "register block, then update the atlas table by hand" footgun.
7. **Threading.** The C++ build uses `std::jthread` with a `std::mutex` to alternate
   between update and render at 30/∞ Hz. Port to a single-threaded fixed-step loop
   driven by `winit`: `RedrawRequested` → step simulation by accumulator in 1/30 s
   slices, then render. Move only chunk gen/IO off-thread, behind `crossbeam_channel`s
   carrying owned `MeshInput` / `MeshOutput` payloads. This eliminates `update_mutex`
   and never crosses thread boundaries with a chunk reference.

### 2.5 Audit of every `Chunk*` use site in the C++ code

Every place the existing code stores or passes a chunk pointer, with the Rust
replacement. Every entry is either an owned snapshot or a `ChunkKey` resolved through
the O(1) grid — none reintroduces a hashmap lookup.

| Site (file:line) | C++ form | Rust replacement |
|------------------|----------|------------------|
| `chunk_pointer_arrays.ixx` whole class | `unique_ptr<Chunk*[]>` sliding window | `ChunkGrid<Option<ChunkKey>>` — O(1) array index, no hash. |
| `worlds.ixx:673` `_chunk_pointer_cache_value` | Single-entry pointer cache for the previous lookup | **Drop entirely.** `ChunkGrid` already gives O(1) coord-keyed access; the single-entry cache stops being useful. |
| `worlds.ixx:148–192` `class RenderData { Chunk* _refer; … }` | Each render entry stores a back-pointer to its chunk so it can read `coord()`, `aabb()`, `updated()` | **Drop the back-pointer.** Collocate render state with the chunk: `ChunkSlot { chunk: Chunk, render: ChunkRender }`. Render-side code reads `slot.chunk.coord()` directly. |
| `worlds.ixx:665` `_chunks: HashMap<ChunkId, unique_ptr<Chunk>>` | Canonical "is this coord loaded?" map | `by_coord: HashMap<IVec3, ChunkKey>` — the same structural role, indexing into the slab. Required because the loaded-chunk set is not always a subset of the grid window (future anchored chunks). |
| `worlds.ixx:666` `_renders: HashMap<ChunkId, unique_ptr<RenderData>>` | Parallel map keyed by chunk id | **Merged into the slab `ChunkSlot`.** The `_renders` map is gone; render state lives next to the chunk. |
| `worlds.ixx:667` `_chunk_meshing_list: vector<pair<int, RenderData*>>` and the local `meshings` priority queue | Transient meshing dispatch list | `Vec<(i32, ChunkKey)>` and `BinaryHeap<(Reverse<i32>, ChunkKey)>`. The list is **rebuilt every frame** by `update_chunk_lists` and consumed in the same frame by `process_chunk_meshings` — no cross-frame staleness, so a plain slab key is safe here. |
| `worlds.ixx:618–630` `process_chunk_meshings` neighbor array `array<Chunk const*, 27>` | 3×3×3 read borrows for greedy meshing | **Two-stage:** (1) main thread reads the 27 `ChunkKey`s via `chunk_grid` (O(1) each), checks all 27 are present, then copies the 18×18×18 padded boundary into an owned `MeshInput { coord: IVec3, padded: Box<[BlockData; 18·18·18]> }`. (2) Worker thread runs `build_mesh(&MeshInput) -> MeshOutput { coord, … }` on the owned snapshot — no chunk references cross the thread boundary. (3) Main thread receives `MeshOutput`, **re-resolves by coord** (`chunk_grid.get(output.coord)`), and writes into `chunks[key].render`. Re-resolution by coord is what neutralizes the slot-reuse hazard for async meshing — a stale `ChunkKey` is never used to index back into the slab. |
| `worlds/chunk_rendering.cpp:158` `ChunkRenderData(ccoord, array<Chunk const*, 27>)` | Same neighbor array, directly indexed during meshing | Replace with `MeshInput` from above. The 18×18×18 padded boundary makes the meshing pass branch-free at the chunk edges. |
| `worlds.ixx:114` `TilesStore::load(Chunk*)` | Coroutine fills the chunk's data on hit | `TilesStore::load(coord) -> Option<Vec<u8>>` — returns owned bytes; caller decodes via `chunk.unpackage_from(&bytes)`. No reference crosses the I/O boundary. |
| `worlds.ixx:128` `TilesStore::save(Chunk*)` | Coroutine reads `chunk.modified()` and `chunk.package_to()` | `TilesStore::save(coord, data: Vec<u8>)`. Caller produces owned `data` via `chunk.package_to()` first; the worker only needs the bytes. |
| `worlds.ixx:261` `World::chunk(ccoord) -> Chunk*` | Public lookup | `World::chunk(&self, ccoord) -> Option<&Chunk>` and `chunk_mut`, both routed through the grid. |
| `worlds.ixx:282/333/413/623/739` `auto cptr = chunk(ccoord)` then `cptr->block(...)` etc. | Internal callers | Same pattern: `if let Some(c) = self.chunk(ccoord) { c.block(bcoord) }` — but after the destructure-borrow trick at the call site, since `update_block` does both reads and writes through this. |
| `worlds.ixx:678` `_load_chunk` returns `Chunk*` | Used by callers to keep working with the just-loaded chunk | Return `ChunkKey` instead. Callers that need `&Chunk` immediately go through `chunks[key]`. |
| `world_rendering.cpp:17` `for (auto const& [_, c]: _renders)` | Iterate render entries to build the visible-chunks list | `for (key, slot) in chunks.iter()` on the slab. Contiguous, cache-friendly, faster than the hashmap iteration. |
| `neworld.ixx:255` `for (auto const& [_, c]: world.chunks())` (random tick) | Iterates all chunks for random block ticking | Same — iterate the slab. The loop body uses `slot.chunk.coord()` and reads via the grid for neighbor checks. |
| `particles.ixx:110` `world.block(...)` | Particle light sampling | Particles consume `&impl BlockView`; the impl forwards to the grid. |
| `player_impl.cpp:88` `world.block_or_air(coord)` | Player physics neighbor read | Same — `&impl BlockView`. |

Two invariants the slab approach relies on (and which `World` enforces):

* **Every cross-frame or cross-thread chunk reference is a coord, not a `ChunkKey`.**
  `ChunkKey`s are only valid within the same frame on the main thread. Anything that
  may outlive a `remove_chunk` call (worker pipelines, save queues, deferred meshing
  results) carries `IVec3` and re-resolves through `chunk_grid.get(coord)` on use.
* **`remove_chunk` clears `chunk_grid` *before* it calls `slab.remove`.** This is the
  Rust counterpart of the C++ "clear `_chunk_pointer_cache_value` on unload"
  discipline (`worlds.ixx:723–727`). With those two clears in the right order, a
  recycled slab slot can never be aliased through the grid.

### 2.6 If we want generation checks back

The slab approach above is unsafe-free but offers no automatic detection if the
two invariants above are ever violated by a future change. If that becomes a concern:

* **`slotmap::SlotMap`** adds a `u32` generation tag per slot. Stale `ChunkKey`s
  fail `chunks.get(key)` rather than aliasing a recycled slot. Cost: one extra
  compare-and-branch per access, and `ChunkKey` grows from 8 bytes to 16. The API
  surface is otherwise identical to slab.
* **Hand-rolled `Vec<Option<Chunk>>` + freelist + `u32` generation per slot.** Same
  tradeoff as `slotmap` but you control the layout.

For the C++-style raw-pointer fast path (no key, no index, no compare):

* **`HashMap<ChunkId, Box<Chunk>>` + a `*const Chunk` cache array** behind a small
  `unsafe` API. `Box` keeps the heap address stable across hashmap rehashes, so
  cached pointers remain valid until the entry is removed. The `unsafe` surface is
  contained to the cache's `set`/`clear` methods, which mirror the existing C++
  unload sequence.

`slab::Slab` is the default recommendation because it matches `World`'s actual
ownership model (the world is the sole allocator and deallocator) without paying for
a defense the design doesn't need.

---

## 3. Recommended Rust crate ecosystem

| Concern | Crate |
|---------|-------|
| Window + events | `winit` (0.30 ApplicationHandler API) |
| Graphics | `wgpu` (0.20+) |
| Math | `glam` |
| UI | `egui` + `egui-winit` + `egui-wgpu` |
| Text rendering (HUD/world) | `glyphon` (built on `cosmic-text` + `wgpu`) |
| PNG load/save | `image` |
| Logging | `tracing` + `tracing-subscriber` |
| Serialization (saves, options) | `serde`, `bincode` (player save), `serde_json` or `toml` (options) |
| Persistence (chunk DB) | `sled` (pure-Rust K/V; replaces LevelDB) |
| Concurrency | `crossbeam-channel`, `rayon` (parallel meshing) |
| Chunk arena | `slab` (key-addressable arena; no GC, no generation tags — `World` is the sole allocator/deallocator). See §2.2. |
| RNG | `rand` (xoshiro for terrain seeding) |
| Errors | `thiserror` for libraries, `anyhow` only at the binary edge |

`wgpu` is the right choice because the existing render module already abstracts GL
behind RAII handles; we are replacing one cross-platform graphics abstraction with
another. WGSL is also tractable — only ~7 small shader pairs plus `final.fsh`.

`sled` over LevelDB removes the C++ ABI dependency that the project currently
work-arounds with FetchContent.

`egui` over a hand-rolled UI saves substantial code (`ui/` is ~1900 lines today) at
the cost of a slightly different visual style; the existing menus map cleanly onto
`egui`'s widgets.

---

## 4. Per-module sub-plans

For each module: scope, replacement, and notes specific to Rust idioms.

### 4.1 `math` → `neworld::math`

* Replace `Vec2/3/4<T>`, `Mat4f`, `Eulerd/f` with `glam::{Vec2,Vec3,Vec4,IVec2,IVec3,Mat4,Quat}`.
  Keep an alias `type Coord = DVec3;` for player/world double-precision positions; `glam`
  has `DVec3` and `DMat4`.
* `AABB<T,N>` → a small `Aabb3<T>` struct with `min: TVec3`, `max: TVec3`. Port
  `intersects`, `clip_displacement`, `extend` directly.
* `Frustum` → port directly (six planes derived from a view-projection matrix).
* `Euler` (heading/pitch/roll) → keep as a struct; provide `to_quat()`/`view_matrix()`
  helpers. Don't substitute `Quat` everywhere — the player code naturally thinks in
  heading/pitch and clamps pitch to `±π/2`.

### 4.2 `blocks.ixx` → `neworld::blocks`

* Newtype `Id(u16)`, `State(u8)`, `Light(u8)` (sky/block packed in nibbles — the
  packing is purely an in-memory choice, no longer needs to match the C++ layout).
* `BlockData { id, state, light }` `#[derive(Copy, Clone, PartialEq, Eq, bytemuck::Pod)]`
  — `Pod` is useful for fast in-memory copies into mesh-input snapshots, not because
  it has to match a C++ on-disk layout.
* `BlockInfo { name: &'static str, solid: bool, opaque: bool, translucent: bool, hardness: f32,
  faces: [TextureIndex; 3] }` — fold the texture-atlas mapping into `BlockInfo`.
* `BlockRegistry` owns a `Vec<BlockInfo>` and is constructed once via
  `register_base_blocks(&mut BlockRegistry) -> BaseBlocks`. After construction it is
  wrapped in `Arc<BlockRegistry>`.
* Drop the `block_info_registry`/`base_blocks` module globals.

### 4.3 `chunks.ixx` → `neworld::chunks`

* `Chunk { coord: IVec3, data: Option<Box<[BlockData; SIZE3]>>, empty: bool, updated: bool, modified: bool }`.
* `block(IVec3u) -> BlockData` — for empty chunks returns
  `BlockData { id: air, light: if y<0 {NO_LIGHT} else {SKY_LIGHT} }` exactly like the
  C++ implementation.
* `block_mut(IVec3u) -> &mut BlockData` — lazily allocates `data`, fills with air, sets
  `_empty = false`.
* `package_to(&self) -> Vec<u8>` / `unpackage_from(&[u8]) -> Result<()>`. Format is
  open: simplest is `bytemuck::cast_slice` over the `BlockData` array, optionally
  preceded by a small header (`u32` magic + `u32` version + flags). Add zstd
  compression behind a feature flag if chunk DB size becomes an issue.
* Terrain init function moves to `worldgen` (see §4.4) and is called as
  `chunk.init_generate(&height_map, &generator)`.

### 4.4 `terrain_generation.ixx` + `height_maps.ixx` → `neworld::worldgen`

* `Generator { perm: [f64; 256], seed: u32 }` owns its permutation table; constructed
  once per world (`World::new` builds it). Ports the existing fractal-noise math
  unchanged.
* `HeightMap` ports directly: `Vec<i32>` of size N×N, `set_center(IVec3)` shifts.
* No module globals; the generator is owned by `World`.

### 4.5 `worlds/player.ixx` → `neworld::player`

* `Player` keeps the same fields. `GameMode` enum. Inventory is `[[ItemStack; 10]; 4]`.
* `update(&mut self, &impl BlockView)` instead of `update(&mut World)` — Player no
  longer needs a world reference for unrelated state (no chunk loading from inside
  the player tick).
* `put_block(&self, &mut World, IVec3, Id) -> bool` — the world is an explicit
  parameter on this one call (it's the only player method that mutates the world).
* `save`/`load`: a `#[derive(Serialize, Deserialize)]` struct encoded with `bincode`
  (or `postcard`). Tag with a `u32` magic + `u32` version so future schema changes can
  be detected; reject unknown versions. C++-era saves will not load and we don't try to
  read them.

### 4.6 `worlds.ixx` + `worlds/*.cpp` → `neworld::world` and `neworld::chunk_grid`

* `World` owns the maps and the player. Layout per §2.2: `chunks: Slab<ChunkSlot>`
  (canonical owner; no GC, key-addressable, deallocated explicitly by `World`),
  `by_coord: HashMap<IVec3, ChunkKey>` (every loaded chunk; required because the
  loaded set is not always a subset of the grid window — see §2.2), `chunk_grid:
  ChunkGrid` (sliding-window hot-path cache). The single-entry
  `_chunk_pointer_cache_{key,value}` micro-cache from the C++ version is dropped —
  the grid array already provides O(1) coord-keyed access without hashing.
* `ChunkGrid` (renamed from `ChunkPointerArray`) is a `Vec<Option<ChunkKey>>` of size
  `(2·(RenderDistance+2))³` plus an `origin: IVec3`. Its `move(offset)` / `set_center`
  reshuffle preserves the C++ behavior; the only difference is what's stored in the
  cells (slab indices instead of raw pointers).
* `ChunkSlot { chunk: Chunk, render: ChunkRender }` collocates the per-chunk render
  state (mesh handles, load anim) with the chunk itself. C++ keeps these in a parallel
  `_renders` map; collocation simplifies lifetime management without changing
  semantics.
* `ChunkKey = usize` is a slab index, scoped to one `World`. **Never persisted, never
  sent across threads as a deferred reference.** Anything that may outlive a
  `remove_chunk` call (worker pipelines, save queues, deferred meshing results)
  carries an `IVec3` coord and re-resolves through `by_coord` (or the grid) on use.
* The chunk DB key on disk is independent of `ChunkKey`. Pick whatever shape is
  simplest for `sled` — e.g. the 12-byte little-endian `[i32; 3]` of the chunk coord,
  or a `postcard`-serialized `IVec3`. The C++ 64-bit packed encoding is no longer
  required (no backward compatibility — see top of §1).
* Insert / remove operations must update **all three** structures (`chunks`,
  `by_coord`, `chunk_grid`). Encapsulate in `World::insert_chunk` /
  `World::remove_chunk` so callers cannot leave them inconsistent. `set_center`
  shifts only the grid; `by_coord` and `chunks` are unaffected (anchored chunks stay
  loaded as the grid slides past them).
* Async chunk load/save: spawn a worker thread (or a small `rayon` pool) at world
  open. Communicate with `crossbeam_channel`:
  * Main → worker: `LoadRequest { coord, response: Sender<LoadResult> }`.
  * Worker → main: `LoadResult { coord, data: Option<Vec<u8>> }`.
  * Main thread completes loads at the start of each frame, calling `init_generate`
    on the result and then `insert_chunk` to wire it into all three structures.
* Async meshing: meshing is pure on the chunk + 26 neighbors. The main thread reads
  the 27 cells via `chunk_grid` (each O(1)), copies their `BlockData` arrays into an
  owned `MeshInput { coord, … }`, and ships it to a worker via `crossbeam_channel`.
  The worker returns a `MeshOutput { coord, opaque: Vec<Vertex>, translucent:
  Vec<Vertex> }`. The main thread re-resolves the coord through `by_coord` (or the
  grid) — never reusing the stale `ChunkKey` from the time the request was queued —
  uploads the buffers, and stores them into `chunks[key].render`.
* `block_update_queue` is `VecDeque<IVec3>`; `update_block` ports straight across,
  using `chunk_grid` for the 6-neighbor lookups (these are the hottest reads in the
  whole simulation tick).
* `hitboxes(box: Aabb3d) -> Vec<Aabb3d>` and `in_water(box) -> bool` port straight,
  reading through the same fast path.
* `list_render_chunks` and `render_chunks` move to the renderer (see §4.10) and
  iterate over `chunks` (the slotmap), which is contiguous and cache-friendly.

### 4.7 `commands.ixx` → `neworld::commands`

* `Command { run: Box<dyn Fn(&[&str], &mut World, &mut Vec<String>) -> bool> }`.
* `CommandRegistry { entries: HashMap<String, Command> }`.
* Argument parsing helpers via `str::parse::<i32>` etc., mirroring `_parse_int`/
  `_parse_float`. Keep the same set of slash-commands.

### 4.8 `globalization.ixx` + `lang/*` → `neworld::i18n`

* `I18n { current: String, lines: Vec<String>, keys: HashMap<String, usize> }`.
* `load(&mut self, lang: &str) -> Result<()>`.
* `get(&self, key: &str) -> &str` — return `""` if missing instead of inserting into
  a map (`std::map::operator[]`'s side effect is a footgun).

### 4.9 `globals.ixx` → `neworld::config` + `neworld::input` + `GameApp` fields

Split the single global bag along three axes:

* `Config` — a serde struct loaded from `configs/options.toml` (TOML over the existing
  ad-hoc INI parser): `fov_y_normal`, `mouse_speed`, `render_distance`, `smooth_lighting`,
  `nice_grass`, `merge_face`, `advanced_render`, `shadow_res`, `max_shadow_distance`,
  `soft_shadow`, `volumetric_clouds`, `ambient_occlusion`, `multisample`, `vsync`,
  `font_scale`, `ui_auto_stretch`, `ui_background_blur`, `language`. Live-edited by the
  options menu.
* `WindowState` — `width`, `height`, `stretch` (DPI-derived), `should_toggle_fullscreen`.
* `InputState` — per-frame: `mouse_pos`, `mouse_motion`, `mouse_wheel_delta`, `mouse_buttons`,
  `keys_down: BitSet`, `keys_pressed: BitSet`, `text_input: String`. Built from `winit`
  events on each `MainEventsCleared` / `AboutToWait`.

Drop `GameTime`, `mw/mb/...`, `inputstr`, `backspace`, `MainWindow`, the chunk counters,
and the RNG seed from globals. `GameTime` belongs to `World`. Counters belong to a
per-frame `Stats` struct on `GameApp`. RNG is per-`Generator` and per-call-site.

### 4.10 `render/*` + `rendering.ixx` + `setup.ixx` (GL parts) → `neworld::gfx`

This is the largest re-design. Replace ~90 KB of C++ template-heavy GL wrappers with a
focused `wgpu` module.

Structure:

* `Gfx` (top-level): owns `wgpu::Instance`, `Adapter`, `Device`, `Queue`, `Surface`,
  surface config. Built once at startup from a `winit::window::Window`.
* `Pipelines` — one struct per pass, each owning its `RenderPipeline` and bind-group
  layouts:
  * `UiPipeline` (handled by `egui-wgpu` for menus; a small custom pipeline for HUD
    quads and breaking overlays)
  * `FilterPipeline` (post-process / blur)
  * `OpaquePipeline` (deferred: writes diffuse/normal/material/depth)
  * `TranslucentPipeline`
  * `FinalPipeline` (the big composition pass — port `final.fsh` to WGSL)
  * `ShadowPipeline`
  * `DebugShadowPipeline`
* `GpuTextures` — wraps `wgpu::Texture` / `TextureView` / `Sampler`. Provides
  `create_2d`, `create_array`, `upload_image`. The block diffuse/normal/noise atlases
  are `wgpu::TextureViewDimension::D2Array` resources.
* `Atlases { block_diffuse, block_normal, block_noise, splash, title, select, unselect }`.
* `FrameUniforms`, `ModelUniforms`, `FilterUniforms` — plain `#[repr(C)]
  bytemuck::Pod` structs, written each frame to a `wgpu::Buffer` with usage
  `UNIFORM | COPY_DST`. This subsumes the `block_layout` template machinery.
* `ChunkMesh` — `{ buffer: wgpu::Buffer, vertex_count: u32 }` for each of the two
  layers (opaque / translucent).
* The WGSL ports of the shaders live in `crates/neworld-gfx/shaders/*.wgsl` and are
  embedded via `include_str!`.
* The greedy meshing code (`worlds/chunk_rendering.cpp`) ports as `mesh_chunk(input:
  &MeshInput) -> MeshOutput` over CPU buffers — no GL/wgpu calls during meshing.
  Upload happens on the main thread.

### 4.11 `text_rendering.ixx` → `neworld::text`

* Replace with `glyphon` (cosmic-text + wgpu). It handles atlas allocation, glyph
  caching, multi-line layout, Unicode shaping. The HUD calls `glyphon::TextRenderer`
  with the same screen coordinates.
* Drop the hand-rolled `UnicodeChar`/`chars` map and the manual atlas image growth.

### 4.12 `textures.ixx` → folded into `neworld::gfx::Atlases` and `neworld::blocks::BlockInfo`

* `LoadTexture`, `LoadBlockTextureArray`, `LoadNormalTextureArray`, `LoadNoiseTextureArray`
  become methods on `Atlases::load_from(&Device, &Queue, paths: &Paths) -> Atlases`.
* `getTextureIndex(Id, face)` becomes `BlockInfo::face(face: usize) -> TextureIndex`.

### 4.13 `particles.ixx` → `neworld::particles`

* `ParticleSystem { particles: Vec<Particle>, view_origin: DVec3 }` owned by `GameApp`,
  not `World`.
* `tick(&mut self, world: &impl BlockView)` for physics; `mesh(&self, view_origin: DVec3,
  interp: f64) -> MeshOutput` for rendering. No globals.
* The "render origin" and "interp" stay explicit args, not stored mutables.

### 4.14 `ui/*` + `menus.ixx` + `menus/*.cpp` → `neworld::ui`

* Replace the entire UI library with `egui`. The menu DSL maps directly:
  * `Column { … }` → `ui.vertical(|ui| …)`
  * `Row { … }` → `ui.horizontal(|ui| …)`
  * `Sizer { max_height: 40 }` → `ui.allocate_ui_with_layout`
  * `Padding { … }` → `egui::Frame::none().inner_margin(...)`
  * `Button { label, on_click }` → `if ui.button(label).clicked() { … }`
  * `Slider`, `TextBox`, `Label`, `ImageBox` are direct egui widgets.
* `ui::Menu` becomes a `trait Screen { fn ui(&mut self, ctx: &egui::Context, app: &mut GameApp) -> Transition; }`.
  `Transition` is `Stay | Push(Box<dyn Screen>) | Pop | Exit`.
* The screen stack lives on `GameApp`. Title/world/options/render/shader/ui-options/language/
  game/create-world menus each become one `Screen` impl, in `src/ui/screens/`.

### 4.15 `setup.ixx` (window / input / fullscreen) → `neworld::input`

* Use `winit`'s `ApplicationHandler` (winit 0.30+).
* `InputState` (see §4.9) is updated on `WindowEvent::{KeyboardInput, MouseInput,
  CursorMoved, MouseWheel, Ime, ReceivedCharacter}`.
* Fullscreen toggle: `Window::set_fullscreen(Some(Fullscreen::Borderless(None)))`.
* DPI: `Window::scale_factor()` replaces `calculate_stretch()`.
* The OpenGL debug callback goes away — `wgpu` validation is enabled via
  `InstanceFlags::VALIDATION` in debug builds.

### 4.16 `neworld.ixx` (the god file) → `neworld::app`

This is the centerpiece of the redesign. Split across:

* `GameApp` — the root struct. Holds everything in §2. Drives the event loop.
* `app::loop` — fixed-step accumulator at 30 ticks/s. On each tick:
  1. `world.set_center(player.coord)` and queue chunk loads.
  2. Random tick (the dirt → grass / grass → dirt logic — moves to `world::random_tick`).
  3. `world.process_block_updates()`.
  4. `hud.update_selection(&world, &input)` — the raycast & breaking-progress logic
     currently in lines 298–423 of `neworld.ixx`.
  5. `chat.update(&input, &mut world, &commands)` if chat is open.
  6. `inventory.update(&input, &mut player)` if inventory is open.
  7. `player.tick(&world.block_view(), &input)`.
  8. `particles.tick(&world.block_view())`.
* `app::frame` — once per render:
  1. `world.update_chunk_lists(player.coord)`.
  2. `world.process_chunk_loads()` (drains the worker channel).
  3. `world.process_chunk_unloads()`.
  4. `world.process_chunk_meshings()` (drains the meshing channel; uploads to GPU).
  5. Renderer issues all passes (shadow, opaque, translucent, post, ui).
  6. Optional readback: screenshot/thumbnail (write PNG to disk).
* `HudState`, `ChatState`, `InventoryState`, `BreakingState` are small structs each
  owning their own bit of state — replacing the `static int` locals scattered through
  `draw_inventory` etc.
* The ~250 lines of vertex-buffer building in `draw_block_selection_border` /
  `draw_block_breaking_texture` / `draw_hud` / `draw_inventory` become methods on the
  HUD/inventory modules feeding the `UiPipeline`.

### 4.17 Misc

* `debug.ixx` → `assert!`/`unreachable!()`/`todo!()`.
* `items.ixx` → `pub struct ItemStack { pub id: blocks::Id, pub count: u8 }` with
  `pub fn empty(&self) -> bool { self.count == 0 }`. (Note: C++ uses `size_t` for
  count, but the inventory math caps at 255; `u8` is correct.)
* `chunk_pointer_arrays.ixx` → `neworld::world::ChunkGrid`. Sliding 3D window of
  `Option<ChunkKey>` (slotmap keys, ~8 bytes). Same `move`/`set_center`/`get`/`set`
  API as the C++ version. See §2.1 / §2.2 / §2.5.

---

## 5. Implementation order (partial)

The following partial order respects "build the leaves first, then the dependents".
Numbered groups can be done in parallel; later groups depend on all earlier ones.

```
[A] foundations   (no deps, do first)
    A1. Cargo workspace skeleton + tracing setup + assets directory layout
    A2. neworld::math        (glam wrappers, Aabb3, Frustum, Euler)
    A3. neworld::config      (Serde TOML options)
    A4. neworld::i18n        (lang loader)
    A5. neworld::input       (InputState struct, no winit binding yet)
    A6. neworld::blocks      (Id/State/Light/BlockData/BlockInfo/BlockRegistry)
    A7. neworld::items

[B] world model   (depends on A)
    B1. neworld::worldgen    (Generator + HeightMap)
    B2. neworld::chunks      (Chunk + package/unpackage)
    B3. neworld::player      (Player struct, save/load via serde)
    B4. neworld::world       (World + TilesStore via sled + block updates,
                              EXCLUDING chunk meshing/render dispatch)
    B5. neworld::commands    (CommandRegistry on top of World)

[C] graphics core (depends on A only)
    C1. winit + wgpu bring-up: window, surface, clear color
    C2. neworld::gfx::Pipelines + WGSL ports of ui/filter/default/opaque/
        translucent/shadow/debug_shadow shaders
    C3. neworld::gfx::Atlases + image loading
    C4. Frame/Model/Filter uniform buffers
    C5. neworld::text via glyphon

[D] world rendering (depends on B + C)
    D1. Mesh builder port (chunk_rendering.cpp greedy meshing → CPU MeshOutput)
    D2. Chunk render upload + draw dispatch
    D3. Particle system + render
    D4. Final pass (port final.fsh)

[E] UI         (depends on A + C)
    E1. egui + egui-winit + egui-wgpu integration
    E2. Screen trait + screen stack
    E3. Main / World / Create World / Game / Options / Render Options /
        Shader Options / UI Options / Language menu screens
    E4. HUD overlay (crosshair, health bar, debug panel, chat)
    E5. Inventory overlay

[F] orchestration (depends on B + D + E)
    F1. neworld::app::GameApp (root struct, fixed-step loop)
    F2. Block selection raycast + breaking
    F3. Chat input + command execution wiring
    F4. Screenshot / thumbnail readback
    F5. Async chunk loading / saving (crossbeam channels + rayon)
    F6. Async chunk meshing
    F7. End-to-end: launch → main menu → enter world → play → save → exit
```

A reasonable single-developer execution sequence: A1→A2→A3→A6→A7→(B1,B2)→B3→B4→
C1→C2→C3→C4→D1→D2→C5→D3→D4→E1→E2→E3→F1→F2→B5→F3→E4→E5→F4→F5→F6→F7.

A two-track plan parallelizes A/B against C as soon as A2 and A6 are done: world
modeling on one track, graphics bring-up on the other; they meet at D1 once both
sides have a stable enough API.

---

## 6. Known risks and open questions

* **No backward compatibility.** All on-disk formats (chunk DB, player save, options,
  language tables) are redesigned freely. Existing C++-built worlds cannot be opened
  by the Rust build, and we don't ship a migration tool. Tag every persisted format
  with a `u32` magic + `u32` version from day one so *future* Rust schema changes have
  a clean upgrade path.
* **LevelDB → sled.** `sled` is a pure-Rust K/V store, removing the C++ ABI dep that
  the project currently works around with `FetchContent`. Key shape is open (see §4.6).
* **Reversed-Z depth.** The current pipeline uses `glClearDepth(0.0f)` +
  `GL_GEQUAL`. `wgpu` supports reversed-Z via `DepthStencilState { depth_compare:
  CompareFunction::Greater, .. }`. Mention this in the `OpaquePipeline` config so we
  don't lose precision.
* **sRGB framebuffer.** The current code enables `GL_FRAMEBUFFER_SRGB`. In `wgpu`
  this is a property of the surface format — pick `TextureFormat::Bgra8UnormSrgb` for
  the surface.
* **`final.fsh` complexity.** 22 KB of GLSL is the largest single shader; budget
  proportionally more time for this WGSL port.
* **Threading model change.** Replacing `update_thread` with a single-threaded
  fixed-step loop is a behavior change: today, render happens in parallel with the
  next tick's compute. If profiling shows this matters, reintroduce a thread pool
  for chunk gen / meshing only (per the F5/F6 plan). The simulation tick itself
  should stay on the main thread for borrow-checker sanity.
