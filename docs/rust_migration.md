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
  in-memory chunk identity is the integer chunk coord `IVec3`, independent of any
  on-disk encoding).
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
| `chunk_pointer_arrays.ixx` | A 3D sliding-window cache of `chunks::Chunk*` keyed by world coord — its purpose is to bypass the chunk hashmap on hot paths | **Not ported.** The Rust port keeps a plain `HashMap<IVec3, Chunk>`; per-access hashing of three `i32`s is cheap, every actual call site looks up by coord anyway, and the bottleneck the C++ cache solved (avoiding a pointer chase on the hot path) is not where the Rust port spends its frame budget. See §2.1 for the rationale and §2.2 for the replacement. |
| `height_maps.ixx` | 2D sliding-window cache of terrain heights | Port directly. |
| `worlds/worlds.ixx` | `World`: chunk hashmap + parallel `_renders` map, `TilesStore` (LevelDB), block-update queue, chunk load/unload/meshing pipelines, render-chunk listing | Solid orchestration; ownership rewires (see §2). The C++ `_chunks` hashmap + parallel `_renders` map + `_chunk_pointer_cache_value` collapse into a plain `HashMap<IVec3, Chunk>`. A second `HashSet<IVec3>` (`non_empty`) tracks the subset whose `Chunk::empty() == false`, so meshing / rendering / save passes are O(non-empty) instead of O(loaded). The chunk-render pipeline (`ChunkMesh`, GPU buffers) lives next door in `Game::chunk_meshes: HashMap<IVec3, ChunkMesh>` rather than collocated in the chunk slot. The transient `_chunk_meshing_list<RenderData*>` becomes a per-frame `Vec<(i32, IVec3)>`. |
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
in Rust without `unsafe`. The port resolves this with two rules:

> **The `World` is the unique owner of all chunk data.** Lookup is a single
> `HashMap<IVec3, Chunk>` access. No long-lived `&Chunk` references into the map are
> stored anywhere; cross-thread / cross-frame references all carry the chunk coord.
>
> **Empty chunks live in the map without allocating block storage.** A parallel
> `HashSet<IVec3>` (`non_empty`) tracks the subset whose `Chunk::empty() == false`,
> so meshing / rendering / save loops are O(non-empty) instead of O(loaded).

### 2.1 Why `HashMap<IVec3, Chunk>` + `HashSet<IVec3>` instead of slab + sliding cache

The C++ design has two complementary lookup paths (which an earlier draft of this plan
proposed to mirror in Rust as a `slab::Slab<ChunkSlot>` + `ChunkGrid<Option<ChunkKey>>`
hot-path cache):

* `_chunks: unordered_map<ChunkId, unique_ptr<Chunk>>` — the canonical owner; one
  hash + one pointer chase per access.
* `ChunkPointerArray` — a 3D sliding window of raw `Chunk*` keyed directly by world
  coord; one integer index, one pointer chase. The whole point of this structure is to
  bypass the hashmap on the hot paths (block-update queue, mesh neighbor reads, player
  hitbox queries).

In the actual Rust port, that layered design didn't pay for itself:

* **No call site needs the cache.** Every chunk access in the Rust port is a coord
  lookup at the call site — there is nowhere in the codebase that holds a `&Chunk`
  long enough for a hashmap rehash to matter. With the raw `Chunk*` cache gone,
  the only thing the sliding-window structure was buying us was avoiding one hash
  per access, on a `HashMap<[i32; 3], _>` whose hash is a few cycles.
* **Empty chunks dominate the loaded set.** Most chunks above the terrain surface
  carry no block data (the existing `Chunk::empty()` flag short-circuits storage
  allocation). The actual hot loops (mesh dispatch, render pass, save-on-exit) want
  to iterate over *non-empty* chunks, not the full loaded set. A sliding-window
  `Option<ChunkKey>` cache doesn't help here — it covers the geometry, not the
  emptiness predicate. A second hash set that mirrors `chunks.keys().filter(non-empty)`
  does.
* **Slot recycling defenses are unnecessary if there are no slots.** The slab+grid
  design earned its complexity from `ChunkKey` reuse: a stale key could alias a
  freshly-recycled chunk, so every async path (mesh worker, save queue) had to
  re-resolve by coord and `remove_chunk` had to clear the grid *before* the slab.
  Going coord-keyed end-to-end deletes that whole class of hazard — there is no
  key to grow stale.

So the port collapses to a single coord-keyed map plus the emptiness side-set:

### 2.2 Layout

```rust
pub struct World {
    name: String,
    dir: PathBuf,
    tiles_store: TilesStore,                       // sled DB handle

    /// Every loaded chunk, keyed by integer chunk coord. Empty chunks
    /// (Chunk::empty() == true) live here too — being in the map means
    /// "this coord is loaded", not "has block data".
    chunks: HashMap<IVec3, Chunk>,

    /// Subset of chunks.keys() whose chunks have allocated block storage.
    /// Invariant: non_empty.contains(c) ⇔ !chunks[c].empty().
    non_empty: HashSet<IVec3>,

    height_map: HeightMap,
    generator: Generator,
    base_blocks: BaseBlocks,
    registry: Arc<BlockRegistry>,
    block_update_queue: VecDeque<IVec3>,
    player: Player,
    game_time: u32,
    render_distance: i32,
    center_ccoord: IVec3,
    pipeline: ChunkPipeline,                       // async load/save worker
    in_flight: HashSet<IVec3>,                     // load requests in flight
}
```

`ChunkRender` does **not** live next to the chunk. The GPU mesh + buffers (`ChunkMesh`)
are owned by `Game::chunk_meshes: HashMap<IVec3, ChunkMesh>`, the same coord-keyed
shape. Splitting them lets the world layer stay graphics-free (tests and the smoke
suite construct a `World` without any `wgpu::Device`) and lets mesh delivery from the
async meshing worker upload through `Game::pump_meshing` without touching `World`'s
fields.

```
GameApp (root)
├── Config / I18n / BlockRegistry (Arc) / Atlases / InputState / Renderer / UiState
├── ParticleSystem      (ticks against &impl BlockView)
├── ChatState           (chatword, history, command registry)
├── HudState            (selx/sely/selz, seldes, bagOpened, FOVyExt)
└── World (Option<World>) — see above
```

### 2.3 Lookup paths and the `with_chunk_mut` invariant

```rust
impl World {
    // Coord-keyed lookup: one hash, one bucket probe.
    pub fn chunk(&self, ccoord: IVec3) -> Option<&Chunk> {
        self.chunks.get(&ccoord)
    }

    // Cheaper than `chunk(c).is_some()` at the call site.
    pub fn is_loaded(&self, ccoord: IVec3) -> bool {
        self.chunks.contains_key(&ccoord)
    }

    // O(non-empty) iteration — the meshing/render/save loops go through this.
    pub fn non_empty_chunks(&self) -> impl Iterator<Item = (IVec3, &Chunk)> {
        self.non_empty.iter().filter_map(|c| self.chunks.get(c).map(|ch| (*c, ch)))
    }
    pub fn non_empty_coords(&self) -> impl Iterator<Item = IVec3> + '_ {
        self.non_empty.iter().copied()
    }

    // O(loaded) — used by tests + the load-list builder.
    pub fn loaded_chunks(&self) -> impl Iterator<Item = (IVec3, &Chunk)> {
        self.chunks.iter().map(|(c, ch)| (*c, ch))
    }

    /// **The only sanctioned path to `&mut Chunk` from `World`.** Hand a
    /// `&mut Chunk` for `ccoord` to `f`, then re-sync `non_empty`. Going
    /// through this guarantees the invariant holds even if `f` triggers a
    /// lazy `Chunk::block_mut` allocation that flips empty → non-empty.
    fn with_chunk_mut<F, R>(&mut self, ccoord: IVec3, f: F) -> Option<R>
    where F: FnOnce(&mut Chunk) -> R,
    {
        let result = self.chunks.get_mut(&ccoord).map(f)?;
        self.refresh_non_empty(ccoord);
        Some(result)
    }

    fn refresh_non_empty(&mut self, ccoord: IVec3) {
        if self.chunks.get(&ccoord).is_some_and(|c| !c.empty()) {
            self.non_empty.insert(ccoord);
        } else {
            self.non_empty.remove(&ccoord);
        }
    }
}
```

The `non_empty` invariant is maintained at exactly three categories of call site:

* **Mutators** (`set_block`, `update_block`, `mark_chunk_neighbor_updated`,
  `unload_chunk*`, `save_to_disk`) all go through `with_chunk_mut(...)` to take their
  `&mut Chunk` borrow. The helper unconditionally calls `refresh_non_empty` after
  the closure returns; the empty bit can flip at most once per chunk lifetime
  (`Chunk::empty` is monotonic-falsy: once false, never true again), so the refresh
  is a one-shot O(1) update.
* **Inserts** (`load_chunk`, `poll_load_results`) own a fresh `Chunk` (not a
  `&mut`), so they call `refresh_non_empty(coord)` directly after `chunks.insert`.
* **Removals** (`unload_chunk`, `unload_chunk_async`) drop from `chunks` and
  `non_empty` atomically as a paired pair, gated on the remove returning `Some`.

The single `chunks.get_mut` call in the file lives inside `with_chunk_mut` itself, so
the invariant cannot be violated by any `&mut Chunk` borrow elsewhere in the codebase.

### 2.4 Why this works for borrows

1. **Mesh building is over an owned snapshot.** `MeshInput { coord: IVec3, padded:
   Box<[BlockData; 18·18·18]> }` is built on the main thread by reading 18×18×18
   cells through `World::block_or_air(coord)`, then shipped to the worker via
   `crossbeam_channel`. The worker never holds a reference into `World`; the main
   thread receives the resulting `MeshOutput { coord, opaque, translucent }`,
   re-checks `World::is_loaded(coord)` (the chunk may have been unloaded mid-flight),
   and uploads via `ChunkMesh::upload`. There is no `ChunkKey` to grow stale.
2. **`Player` lives inside `World`.** `Player::update(world)` needs both `&mut
   Player` and `&BlockView` for the world. Rust's borrow checker can't see that
   these are field-disjoint, so `World::update_player` does the split via
   `mem::take`:
   ```rust
   pub fn update_player(&mut self) {
       let mut player = std::mem::take(&mut self.player);
       player.update(&*self);   // &World coerces to &dyn BlockView
       self.player = player;
   }
   ```
   The default-sentinel player that lives in the slot during the call is never
   observed by the world's block lookups.
3. **`BlockView` trait** abstracts read-only block lookup. Implemented for
   `World` directly; particles, player physics, and HUD raycasts all consume
   `&impl BlockView` instead of `&mut World`.
4. **No global statics for game state.** `App` is created in `main`, threaded
   through `winit`'s event loop (the `ApplicationHandler` in winit 0.30+), and
   dropped on exit. Renderer pipelines live on `App.gfx` / `Game.chunk_pipeline`,
   not in a `Renderer::` namespace.
5. **Block registry is read-only after init.** Hold it as `Arc<BlockRegistry>` in
   `App` and clone the `Arc` into worker threads. No interior mutability needed.
6. **Texture atlas indices live with the registry.** The `TextureIndex` enum and the
   per-block `[face0, face1, face2]` table fold into `BlockInfo`. Eliminates the
   "register block, then update the atlas table by hand" footgun.
7. **Threading.** The C++ build uses `std::jthread` with a `std::mutex` to alternate
   between update and render at 30/∞ Hz. Port to a single-threaded fixed-step loop
   driven by `winit`: `RedrawRequested` → step simulation by accumulator in 1/30 s
   slices, then render. Move only chunk gen/IO and meshing off-thread, behind
   `crossbeam_channel`s carrying owned payloads (`MeshInput` / `MeshOutput` / chunk
   bytes). No chunk references cross thread boundaries.

### 2.5 Audit of every `Chunk*` use site in the C++ code

Every place the existing C++ code stores or passes a chunk pointer, with the Rust
replacement. Every entry is an owned snapshot or a coord lookup; none stores a
long-lived chunk reference.

| Site (file:line) | C++ form | Rust replacement |
|------------------|----------|------------------|
| `chunk_pointer_arrays.ixx` whole class | `unique_ptr<Chunk*[]>` sliding window | **Not ported.** A plain `HashMap<IVec3, Chunk>` lookup is fast enough; see §2.1. |
| `worlds.ixx:673` `_chunk_pointer_cache_value` | Single-entry pointer cache for the previous lookup | **Drop entirely.** No cache layer below the hash map. |
| `worlds.ixx:148–192` `class RenderData { Chunk* _refer; … }` | Each render entry stores a back-pointer to its chunk so it can read `coord()`, `aabb()`, `updated()` | **Drop the back-pointer.** GPU mesh state lives in `Game::chunk_meshes: HashMap<IVec3, ChunkMesh>`; the chunk's own `coord()` / `aabb()` are read via the world's coord lookup when needed. |
| `worlds.ixx:665` `_chunks: HashMap<ChunkId, unique_ptr<Chunk>>` | Canonical "is this coord loaded?" map | `World::chunks: HashMap<IVec3, Chunk>` — the same role, with `Chunk` stored inline (no `Box`; the `Chunk` struct is small and lazy-allocates its block array on first write). |
| `worlds.ixx:666` `_renders: HashMap<ChunkId, unique_ptr<RenderData>>` | Parallel map keyed by chunk id | **Moved out of `World`** into `Game::chunk_meshes: HashMap<IVec3, ChunkMesh>`. Same coord-keyed shape; the world layer stays graphics-free. |
| `worlds.ixx:667` `_chunk_meshing_list: vector<pair<int, RenderData*>>` and the local `meshings` priority queue | Transient meshing dispatch list | `Game::dirty_chunks: HashSet<IVec3>`. Each frame `pump_meshing` snapshots dirty coords into `Vec<(i32, IVec3)>` (squared distance to the player), sorts ascending, and submits the closest 8 to the mesh worker. |
| `worlds.ixx:618–630` `process_chunk_meshings` neighbor array `array<Chunk const*, 27>` | 3×3×3 read borrows for greedy meshing | **Two-stage:** (1) main thread reads the 18×18×18 padded boundary into an owned `MeshInput { coord: IVec3, padded: Box<[BlockData; 18·18·18]> }` via 5 832 calls to `World::block_or_air(coord)`. (2) Worker thread runs `mesh_chunk(&MeshInput) -> MeshOutput { coord, … }` — no chunk references cross the thread boundary. (3) Main thread receives `MeshOutput`, re-checks `World::is_loaded(output.coord)`, and uploads via `ChunkMesh::upload`. |
| `worlds/chunk_rendering.cpp:158` `ChunkRenderData(ccoord, array<Chunk const*, 27>)` | Same neighbor array, directly indexed during meshing | Replace with `MeshInput` from above. The 18×18×18 padded boundary makes the meshing pass branch-free at the chunk edges. |
| `worlds.ixx:114` `TilesStore::load(Chunk*)` | Coroutine fills the chunk's data on hit | `TilesStore::load(coord) -> Option<Vec<u8>>` — returns owned bytes; caller decodes via `chunk.unpackage_from(&bytes)`. No reference crosses the I/O boundary. |
| `worlds.ixx:128` `TilesStore::save(Chunk*)` | Coroutine reads `chunk.modified()` and `chunk.package_to()` | `TilesStore::save(coord, data: &[u8])`. Caller produces owned `data` via `chunk.package_to()` first; the worker only needs the bytes. |
| `worlds.ixx:261` `World::chunk(ccoord) -> Chunk*` | Public lookup | `World::chunk(&self, ccoord) -> Option<&Chunk>`. There is no `chunk_mut`; mutations go through `World::with_chunk_mut(coord, |c| {...})`. |
| `worlds.ixx:282/333/413/623/739` `auto cptr = chunk(ccoord)` then `cptr->block(...)` etc. | Internal callers | Same pattern: `if let Some(c) = self.chunk(ccoord) { c.block(bcoord) }`. |
| `worlds.ixx:678` `_load_chunk` returns `Chunk*` | Used by callers to keep working with the just-loaded chunk | Returns `()`; the chunk lives in `self.chunks` and callers re-look it up by coord if needed (no caller does). |
| `world_rendering.cpp:17` `for (auto const& [_, c]: _renders)` | Iterate render entries to build the visible-chunks list | `for (coord, mesh) in &game.chunk_meshes` on the GPU mesh map. |
| `neworld.ixx:255` `for (auto const& [_, c]: world.chunks())` (random tick) | Iterates all chunks for random block ticking | `for (coord, chunk) in world.non_empty_chunks()` — pure-air sky chunks have nothing to randomly tick, so iterating only the non-empty subset is both faster and correct. |
| `particles.ixx:110` `world.block(...)` | Particle light sampling | Particles consume `&impl BlockView`; the impl forwards to `World::block_or_air(coord)`. |
| `player_impl.cpp:88` `world.block_or_air(coord)` | Player physics neighbor read | Same — `&impl BlockView`. |

The two structural invariants `World` enforces:

* **Every cross-frame or cross-thread chunk reference is a coord.** The mesh worker
  ships `MeshInput { coord, … }` and returns `MeshOutput { coord, … }`; the save
  pipeline ships `(coord, bytes)`; the load pipeline ships `LoadResult { coord,
  chunk }`. The main thread re-checks `World::is_loaded(coord)` before acting on a
  result, so a chunk that was unloaded mid-flight is dropped instead of revived.
* **`non_empty.contains(c) ⇔ !chunks[c].empty()`** at all times. Maintained by
  funnelling every `&mut Chunk` borrow through `World::with_chunk_mut`, plus paired
  removals in `unload_chunk` / `unload_chunk_async`. Documented at the helper's
  definition; auditable with one grep for `chunks.get_mut`.

---

## 3. Recommended Rust crate ecosystem

| Concern | Crate |
|---------|-------|
| Window + events | `winit` (0.30 ApplicationHandler API) |
| Graphics | `wgpu` (0.20+) |
| Math | `cgmath` (chosen over `glam` so `Aabb3<S>` / `Euler<S>` / `Frustum<S>` can be true scalar generics — `glam`'s `Vec3` and `DVec3` are unrelated concrete types that would force macro-generated `Aabb3f` / `Aabb3d` pairs) |
| UI | `egui` + `egui-winit` + `egui-wgpu` |
| Text rendering (HUD/world) | `glyphon` (built on `cosmic-text` + `wgpu`) |
| PNG load/save | `image` |
| Logging | `tracing` + `tracing-subscriber` |
| Serialization (saves, options) | `serde`, `bincode` (player save), `serde_json` or `toml` (options) |
| Persistence (chunk DB) | `sled` (pure-Rust K/V; replaces LevelDB) |
| Concurrency | `crossbeam-channel`, `rayon` (parallel meshing) |
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

* Replace `Vec2/3/4<T>`, `Mat3/4<T>`, `Eulerd/f` with re-exports of
  `cgmath::{Vector2,Vector3,Vector4,Matrix3,Matrix4,Quaternion}`. cgmath's vectors
  and matrices are parametric over the scalar (`f32` / `f64` and beyond), so
  `Aabb3<S>` / `Euler<S>` / `Frustum<S>` can be true scalar generics. (We picked
  cgmath over glam for exactly that reason — glam's `Vec3` and `DVec3` are
  unrelated concrete types that would force a macro hack.) Keep
  `type Coord = Vector3<f64>` for player/world double-precision positions, plus
  C++-style flavour aliases (`Vec3i`, `Vec3f`, `Vec3d`, `Mat4f`, …) so call sites
  port without renaming.
* `AABB<T,N>` → `Aabb3<S: BaseFloat>` with `min: Vector3<S>`, `max: Vector3<S>`.
  Port `intersects`, `clip_displacement`, `extend` directly. `Aabb3f` / `Aabb3d`
  are aliases for `Aabb3<f32>` / `Aabb3<f64>`.
* `Frustum` → `Frustum<S: BaseFloat>` (six clip-plane equations as `Vector4<S>`),
  with `from_mvp(&Matrix4<S>)` and `test(&Aabb3<S>) -> bool`.
* `Euler` (heading/pitch/roll) → `Euler<S: BaseFloat>`; keep as a struct with
  `direction()`, `matrix()`, `view_matrix()`, `to_quat()`, `normalize()`,
  `normalize_player()` (clamps pitch to `±π/2`). Don't substitute `Quaternion`
  everywhere — the player code naturally thinks in heading/pitch.

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

### 4.6 `worlds.ixx` + `worlds/*.cpp` → `neworld::worlds`

* `World` owns the chunk map, the non-empty side-set, and the player. Layout per §2.2:
  `chunks: HashMap<IVec3, Chunk>` (every loaded chunk, keyed by coord — being in the
  map means "loaded", not "has data"), `non_empty: HashSet<IVec3>` (the subset whose
  `Chunk::empty() == false`). The single-entry `_chunk_pointer_cache_{key,value}` micro-
  cache, the `_chunks` hashmap + parallel `_renders` map, and the
  `chunk_pointer_arrays` sliding window all collapse into the single hash map.
* In-memory chunk identity is the `IVec3` chunk coord throughout. There is no
  `ChunkKey` / slab index; cross-thread payloads (`LoadResult { coord, chunk }`,
  `MeshOutput { coord, … }`, save bytes) all carry the coord and re-resolve via
  `World::is_loaded(coord)` after the worker returns.
* Per-chunk GPU render state (mesh handles, vertex buffers) lives **outside** `World`
  in `Game::chunk_meshes: HashMap<IVec3, ChunkMesh>`. The world layer stays
  graphics-free (tests + the smoke suite construct a `World` without any
  `wgpu::Device`); mesh delivery from the async meshing worker uploads through
  `Game::pump_meshing` without touching `World`'s fields.
* The chunk DB key on disk is independent of in-memory identity. Pick whatever shape
  is simplest for `sled` — currently the 12-byte little-endian `[i32; 3]` of the
  chunk coord. The C++ 64-bit packed encoding is no longer required (no backward
  compatibility — see top of §1).
* `World` exposes one mutable-chunk path: `with_chunk_mut(coord, |chunk| { … })`. The
  helper takes the `&mut Chunk` borrow, runs the closure, and re-syncs `non_empty`
  before returning. Inserts (`load_chunk`, `poll_load_results`) own a fresh `Chunk`
  (not a `&mut`) and call `refresh_non_empty(coord)` directly after `chunks.insert`;
  removals (`unload_chunk`, `unload_chunk_async`) drop from both maps atomically.
  This makes the `non_empty.contains(c) ⇔ !chunks[c].empty()` invariant trivially
  auditable — one grep for `chunks.get_mut` shows the only call site is inside
  `with_chunk_mut`.
* Async chunk load/save: spawn a worker thread (or a small `rayon` pool) at world
  open. Communicate with `crossbeam_channel`:
  * Main → worker: `LoadRequest::Load(IVec3)` or `LoadRequest::Save(IVec3, Vec<u8>)`.
  * Worker → main: `LoadResult { coord: IVec3, chunk: Chunk }`.
  * Main thread drains results in `World::poll_load_results`, inserts via
    `chunks.insert(coord, chunk)`, and calls `refresh_non_empty(coord)`.
* Async meshing: meshing is pure on the chunk + 26 neighbors. The main thread reads
  the 18×18×18 padded boundary into an owned `MeshInput { coord, padded }` via
  `World::block_or_air(coord)` calls (one hash per cell — cheap), and ships it to a
  worker via `crossbeam_channel`. The worker returns a `MeshOutput { coord, opaque:
  Vec<Vertex>, translucent: Vec<Vertex> }`. The main thread re-checks
  `World::is_loaded(output.coord)` (the chunk may have been unloaded mid-flight),
  uploads the buffers, and stores them in `Game::chunk_meshes`.
* `block_update_queue` is `VecDeque<IVec3>`; `update_block` ports straight across.
* `hitboxes(box: Aabb3d) -> Vec<Aabb3d>` and `in_water(box) -> bool` port straight.
* The C++ `list_render_chunks` / `render_chunks` move to the renderer (see §4.10).
  The chunk-iteration source is `World::non_empty_chunks()` — pure-air sky chunks
  above the terrain don't get scanned just to confirm they have nothing to draw.

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
* `chunk_pointer_arrays.ixx` → **not ported.** The Rust port keeps a plain
  `HashMap<IVec3, Chunk>`; per-access hashing of three `i32`s is cheap, and a
  parallel `HashSet<IVec3>` (`non_empty`) gives O(non-empty) iteration over the
  chunks meshing / rendering / save loops actually care about. See §2.1.

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
