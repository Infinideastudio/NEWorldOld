# WorldGen 合入方案

@JelawatIHPC

将独立地形生成器 [`D:\Rust\WorldGen`](https://github.com/) (`terrain_gen` crate) 合入 NEWorld 的最终实施方案。本文档为方案规约与 PR 计划；实现工作在 `feat/worldgen` 分支上完成。

---

## 1. 架构总览

```
┌─────────────────────────────────────────────────────────────────┐
│  主线程（game tick）                                             │
│                                                                 │
│  RangeLoader::tick_chunk_loading                                │
│    ├─ drain terrain_rx → 存 HeightMapCache（瓦片高度图缓存）       │
│    └─ 对每个候选区块：                                            │
│        ├─ tile 在缓存中 → 从缓存采样列高 → init_generate → load_chunk │
│        └─ tile 不在缓存 → 发 WorldGenRequest{tile} 到 terrain_tx  │
│                                                                 │
│  init_generate：Rock/Dirt/Grass/Sand/Water/Air 分层规则（复用）    │
└──────────────────────┬──────────────────────────────────────────┘
                       │ crossbeam-channel
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  地形线程 neworld-worldgen-worker（串行队列）                    │
│                                                                 │
│  收到 WorldGenRequest{tile_i, tile_j}：                         │
│    1. noise_map_chunkbase(seed, tile_i, tile_j) → N×N Perlin    │
│    2. erode(N×N, 20×71) → 侵蚀后高度图                         │
│    3. 提取内部 M×M → 发回 WorldGenResult{tile, heightmap}       │
└─────────────────────────────────────────────────────────────────┘
```

分块参数：

| 参数 | 格 | 区块 |
|------|-----|------|
| M（提交区域边长） | 512 | 32 |
| margin（光晕宽度） | 64 | 4 |
| N = M + 2×margin | 640 | 40 |
| 结果利用率 | M²/N² = 64% | — |

---

## 2. 模块移植清单

完全替换 NEWorld 现有地形生成——包括噪声层，不保留旧 `HeightNoise`。

### 2.1 移入 `src/core/game/worldgen/` 的模块

| WorldGen 源文件 | 目标位置 | 说明 |
|----------------|---------|------|
| `perlin_noise/lattice.rs` | `worldgen/perlin/lattice.rs` | 哈希梯度索引噪声 + 区域缓冲预计算 |
| `perlin_noise/perlin.rs` | `worldgen/perlin/perlin.rs` | 真·Perlin 噪声（32 梯度，六次多项式插值） |
| `terrain/noise_map.rs` | `worldgen/perlin/noise_map.rs` | 6 倍频 Perlin 叠加，改为按瓦片区域采样 |
| `erosion/smooth_spike.rs` | `worldgen/erosion/smooth_spike.rs` | 侵蚀修正系数函数 |
| `erosion/erode.rs` | `worldgen/erosion/erode.rs` | 水力侵蚀（坡度权重 + 水量迭代 + 高度修正） |

### 2.2 删除的旧模块

- `worldgen/noise.rs`（`HeightNoise` + `noise_2d` + `fractal_noise_2d`）—— 整体移除，不再保留。

### 2.3 保留不动

- `worldgen/terrain_generator.rs::init_generate` —— 方块分层规则（rock/dirt/grass/sand/water/air/bedrock），待高度来源替换后微调（见 §4.1）。
- `worldgen/terrain_generator.rs::TerrainGenerator` —— 重写（见 §4.3）。
- `worldgen/mod.rs` —— 重写（见 §4.3）。

---

## 3. 数据流细节

### 3.1 瓦片坐标与采样

世界按 M×M 区块（512×512 格）为 tile 分割。tile 坐标 `(ti, tj)` 与世界格坐标的关系：

```
tile 格原点 = (ti * 512, tj * 512)
tile 格范围 = [ti*512, ti*512+511] × [tj*512, tj*512+511]
```

`noise_map_chunkbase` 需重构为接受 `(world_origin_x, world_origin_y, region_w, region_h, seed, zoom)`，对任意矩形区域采样。**关键约束**：不同 tile 的采样点必须落在同一世界坐标系下——保证 Perlin 在 tile 边界上连续（无噪声接缝）。

### 3.2 噪声→列高转换

WorldGen `noise_map_chunkbase` 输出 f32（约 [-2, 2]），`erode` 后值域基本不变。NEWorld `init_generate` 需要 i32 列高（世界 y 坐标）。

Perlin 噪声以 **0 为海平面**（噪声均值附近为 0），NEWorld 的 `WATER_LEVEL = 96`。因此直接将 0 对齐到 96：

```rust
const WATER_LEVEL: i32 = 96;
const WORLDGEN_HEIGHT_SCALE: f32 = 144.0;
const WORLDGEN_HEIGHT_OFFSET: f32 = 96.0;

fn height_to_block_y(noise_value: f32) -> i32 {
    (noise_value * WORLDGEN_HEIGHT_SCALE + WORLDGEN_HEIGHT_OFFSET).round() as i32
}
```

采用固定线性映射：

- 噪声值 `-1.0` → 世界高度 `y=-48`
- 噪声值 `0.0` → 世界高度 `y=96`（海平面）
- 噪声值 `1.0` → 世界高度 `y=240`

水平方向上，基础 Perlin octave 的 1 个噪声坐标单位对应世界 512 个方块，即每个方块的基础噪声坐标增量为 `1.0 / 512.0`。后续 octave 仍按 2 倍频率递增，同时按对应 amplitude 衰减贡献。

### 3.3 高度图缓存（`HeightMapCache`）

存储已侵蚀的 M×M f32 高度图，key 为 tile 坐标：

```rust
struct TileData {
    heights: [[f32; 512]; 512],  // 512×512 格的侵蚀后高度
}

struct HeightMapCache {
    tiles: DashMap<Vec3i, TileData>,
}
```

- 主线程持有 `Arc<HeightMapCache>`。
- 地形线程算完后通过 `crossbeam-channel` 回传 `WorldGenResult { tile: Vec3i, data: TileData }`，主线程 drain 时写入缓存。
- 缓存只在内存中；区块从 sled 恢复时走 `try_load_from_disk`，不走 heightmap 缓存。

### 3.4 方块分层规则（`init_generate` 改动）

现有 `init_generate` 接收 `&HeightNoise` 返回 i32 列高。替换为：

```rust
pub(super) fn init_generate(
    blocks: &mut ChunkData,
    coord: Vec3i,
    cache: &HeightMapCache,      // 替换 noise: &HeightNoise
    base: &BaseBlocks,
)
```

内部对每个 `(x, z)` 列：
1. 从 `HeightMapCache` 采样该列的 f32 侵蚀高度。
2. `height_to_block_y(h)` → `i32` 列高 `h`。
3. 按原有规则分层放置方块（rock/dirt/grass/sand/water/air/bedrock），`WATER_LEVEL = 96` 不变。

`collect_heights` 函数同步改为从 `HeightMapCache` 采样。

---

## 4. 线程模型

### 4.1 地形线程（`worldgen-worker`）

- 线程名：`"neworld-worldgen-worker"`。
- 生命周期：`Game::new` 时 spawn，`Game::drop` 时通过 drop channel 通知退出并 join。
- 串行队列：一次只算一个瓦片，收到 `WorldGenRequest` → 计算 → 回传 `WorldGenResult` → 等下一个请求。
- 通道：`crossbeam_channel::unbounded`（与 `MeshPipeline` 同模式）。

```rust
struct WorldGenRequest {
    tile: Vec3i,           // tile 坐标
}

struct WorldGenResult {
    tile: Vec3i,
    data: TileData,        // M×M 侵蚀后 f32 高度图
}
```

### 4.2 主线程消费流程

`range_loader.rs` 改为两阶段：

```
tick_chunk_loading(world, terrain_tx, terrain_rx, heightmap_cache, pending_tiles):
  Phase 1: drain terrain_rx → heightmap_cache.insert(tile, data)，从 pending_tiles 移除对应 tile
  Phase 2: 对每个候选区块 cc：
    tile = cc / 512（x,z 分别整除）
    if tile in heightmap_cache:
        world.load_chunk(cc, || init_generate_from_cache(cc, cache, base))
    elif tile not in pending_tiles:
        terrain_tx.send(WorldGenRequest{tile})
        pending_tiles.insert(tile)
    // else: 已在请求中，跳过，等下次 tick
```

- 主线程不阻塞：cache miss 时发请求即返回，区块保持未加载。
- 瓦片就绪前，该瓦片内的区块保持缺失状态——玩家靠近时下一次 tick 会重新尝试。
- `pending_tiles: HashSet<Vec3i>` 防重复发请求。

### 4.3 通道回传与同步

```
主线程                           地形线程
  │                                │
  │── WorldGenRequest{tile} ──────▶│
  │                                │ 计算 Perlin + erode
  │                                │（串行，一次一瓦片）
  │◀── WorldGenResult{tile,data} ─│
  │ drain → cache.insert           │
  │ pending_tiles.remove           │
  │                                │
  │── WorldGenRequest{tile} ──────▶│
  │                                │
```

`MeshPipeline` 已有的 drain 模式可直接复用：`terrain_rx.try_recv()` 在每 tick 主循环里非阻塞调用。

---

## 5. Seed 持久化（前置必要项）

`Metadata`（`world.dat`）v1 只存 `block_mapping`。需升 v2 加 `seed: u32`：
- 新世界写入菜单传入 seed。
- 旧世界 v1 读入时回退 `derive_seed(name)` 并写回 v2。
- `WorldAction::Enter` 优先用存档 seed。
- 侵蚀缓存按 `(world_seed, tile_i, tile_j)` 索引——seed 变则缓存失效。

同时关闭 README "worldgen seed wiring" 待办。

---

## 6. PR / 分支计划

分支：`feat/worldgen`（off `main`）。遵循仓库 conventional-commit 风格。

| # | 提交 | 内容 |
|---|------|------|
| 1 | `docs(worldgen): integration plan` | 本文档 |
| 2 | `feat(worldgen): port Perlin + lattice noise in-tree` | 搬入 `perlin/{lattice,perlin}.rs`，适配 NEWorld 风格（固定数组、去 `Vec<Vec>`、`cgmath` 坐标）。去掉 `rand`/`rand_mt`/`clap`（倍频 seed 用哈希派生）。附 Perlin 确定性测试 |
| 3 | `feat(worldgen): add Perlin noise_map for arbitrary regions` | 搬入 `perlin/noise_map.rs`，重构为按任意矩形区域采样（非固定 400×400）。附跨 tile Perlin 连续性测试 |
| 4 | `feat(worldgen): port hydraulic erosion` | 搬入 `erosion/{smooth_spike,erode}.rs`，适配固定数组。附侵蚀确定性测试 |
| 5 | `feat(worldgen): persist world seed in world.dat (v2)` | `Metadata` v2 + seed 字段 + v1→v2 兼容读。附往返测试 |
| 6 | `feat(worldgen): replace HeightNoise + add HeightMapCache` | 删除旧 `noise.rs`，`init_generate` 改为从 `HeightMapCache` 采样，加 `height_to_block_y` 转换，加 `HeightMapCache` 结构体 |
| 7 | `feat(worldgen): add worldgen-worker thread` | spawn 地形线程、通道、drain 逻辑、`pending_tiles` 防重、`WorldRequest`/`WorldResult` 类型 |
| 8 | `feat(worldgen): rewire RangeLoader for async tile loading` | 两阶段 tick（drain → load）、闭包改为 `init_generate_from_cache` |
| 9 | `feat(worldgen): rewire Game + drop protocol` | `Game::new` spawn 地形线程、`Game::drop` 通知退出 join |

每个提交须通过：
```sh
cargo clippy --all-targets -- -D warnings
cargo test
```

---

## 7. 已决事项与待调参数

以下为实施时需注意的已决事项，不再需要讨论：

- **`rand_mt` 去掉**：倍频 seed 用哈希派生，不新增 RNG 依赖。
- **WorldGen `Chunk` 类型内聚到 `perlin` 模块私有**：避免与 `core::world::Chunk` 冲突。
- **瓦片边界 Perlin 连续性**：实验已验证在可接受范围内。
- **margin=64**：实验确认接缝质量达标。
- **侵蚀步数**：保持 20 步，单步侵蚀系数为 `0.003`（地形线程不卡主线程）。

已确定的坐标映射：

- **水平尺度**：基础 Perlin 坐标 1.0 单位对应世界 512 个方块。
- **垂直尺度**：噪声值 `-1.0/0.0/1.0` 分别映射到世界 `y=-48/96/240`。
- **基岩以下**：世界 `y<0` 一律填充 rock，避免低地形在基岩下方生成水。
