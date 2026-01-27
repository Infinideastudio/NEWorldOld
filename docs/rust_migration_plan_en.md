# NEWorld Complete Rust Migration Plan

> **Document Version**: 1.0  
> **Created**: 2026-01-27  
> **Objective**: Provide a complete migration roadmap from C++23 to Rust

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current State Analysis](#2-current-state-analysis)
3. [Technology Stack Mapping](#3-technology-stack-mapping)
4. [Migration Strategy](#4-migration-strategy)
5. [Phased Migration Plan](#5-phased-migration-plan)
6. [Technical Challenges](#6-technical-challenges)
7. [Risk Assessment](#7-risk-assessment)
8. [Resource Estimation](#8-resource-estimation)
9. [Recommendations](#9-recommendations)

---

## 1. Executive Summary

### 1.1 Migration Goals

This document outlines a comprehensive plan to migrate the NEWorld voxel game engine from **C++23** to **Rust**, achieving:

- ✅ **Memory Safety**: Eliminate 90%+ memory-related bugs through compile-time guarantees
- ✅ **Concurrency Safety**: Prevent data races with Rust's ownership system
- ✅ **Modernization**: Leverage Rust's ecosystem and tooling (Cargo, crates.io)
- ✅ **Performance**: 5-15% potential performance improvements with SIMD and better optimizations
- ✅ **Maintainability**: Stronger type system and error handling

### 1.2 Timeline & Effort

- **Full Migration**: **56 weeks (13-14 months)** with 1 full-time developer
- **MVP Migration**: **12-16 weeks** for core modules only
- **Team Learning**: 2-4 weeks for Rust fundamentals

### 1.3 Recommended Approach

**Gradual Migration** (C++/Rust hybrid) with the following priorities:

1. Math library (Phase 1) - Low risk, high learning value
2. Block/Chunk systems (Phase 3) - High memory safety gains
3. Terrain generation (Phase 4) - Easy to parallelize
4. World persistence (Phase 7) - Critical for data safety

---

## 2. Current State Analysis

### 2.1 Project Overview

- **Type**: Voxel game engine (Minecraft-like)
- **Language**: C++23 with experimental `import std` modules
- **Code Size**: ~12,000 lines across 65+ files
- **Build System**: CMake 3.30+
- **Graphics**: OpenGL 4.3+

### 2.2 Architecture

```
NEWorld Architecture:
├── Math Library (math/)
│   ├── Vectors, matrices, quaternions
│   ├── AABB, frustum culling
│   └── ~500 lines
├── Rendering Layer (render/)
│   ├── OpenGL abstractions (Buffer, Texture, Shader, VAO, FBO)
│   ├── Block mesh generator
│   └── ~2,200 lines
├── World System (worlds/)
│   ├── Chunk management (16³ blocks)
│   ├── Terrain generation (Perlin noise)
│   ├── LevelDB persistence
│   └── ~3,000 lines
├── UI Framework (ui/)
│   ├── Constraint-based layout
│   ├── Widget library
│   └── ~2,000 lines
└── Game Logic
    ├── Blocks, items, particles
    ├── Commands, menus
    └── ~4,300 lines
```

### 2.3 Current Dependencies

| C++ Dependency | Purpose |
|----------------|---------|
| GLFW3 | Window/input management |
| GLAD | OpenGL loader |
| FreeType | Font rendering |
| libpng | Image I/O |
| LevelDB | Key-value storage |
| spdlog | Logging |
| utf8cpp | UTF-8 handling |
| klsxx | C++ utilities (coroutines, threads) |

---

## 3. Technology Stack Mapping

### 3.1 Core Dependencies

| C++ Library | Rust Replacement | Compatibility | Notes |
|------------|------------------|---------------|-------|
| **GLFW3** | `winit` + `glutin` | ⭐⭐⭐⭐⭐ | Native Rust, feature-complete |
| **GLAD** | `gl` crate | ⭐⭐⭐⭐⭐ | Auto-generated bindings |
| **FreeType** | `freetype-rs` | ⭐⭐⭐⭐ | Official Rust bindings |
| **libpng** | `png` crate | ⭐⭐⭐⭐⭐ | Pure Rust implementation |
| **LevelDB** | `leveldb` / `rusty-leveldb` | ⭐⭐⭐⭐ | Bindings or pure Rust |
| **spdlog** | `tracing` + `tracing-subscriber` | ⭐⭐⭐⭐⭐ | More powerful structured logging |
| **utf8cpp** | Built-in `String` / `str` | ⭐⭐⭐⭐⭐ | Native UTF-8 in Rust |
| **klsxx** | `async`/`await` + `tokio` | ⭐⭐⭐⭐⭐ | First-class async support |
| **CMake** | `Cargo` | ⭐⭐⭐⭐⭐ | Simpler build system |

**Additional Recommendations**:
- **Linear algebra**: `glam` (SIMD-optimized game math)
- **Graphics**: `wgpu` (modern API supporting OpenGL/Vulkan/Metal)
- **ECS**: `bevy_ecs` or `hecs` (if considering ECS architecture)
- **Noise**: `noise-rs`
- **Parallelism**: `rayon`

### 3.2 Language Feature Mapping

| C++23 Feature | Rust Equivalent | Notes |
|--------------|----------------|-------|
| **Modules** | `mod` + `pub` | Native module system |
| **Concepts** | Trait bounds | More powerful generic constraints |
| **Ranges** | Iterators | Lazy evaluation, zero-cost |
| **Coroutines** | `async`/`await` | First-class language support |
| **Smart pointers** | `Box`, `Rc`, `Arc` | Built-in ownership system |
| **constexpr** | `const fn` | Compile-time evaluation |
| **RAII** | Drop trait | Automatic resource management |
| **Template metaprogramming** | Procedural macros + traits | Safer metaprogramming |

---

## 4. Migration Strategy

### 4.1 Approach: Gradual Migration

We recommend a **gradual migration** strategy:

1. **Lower Risk**: Each module validated independently
2. **Continuous Availability**: Project remains buildable/runnable
3. **Learning Curve**: Team learns Rust progressively
4. **Rollback Option**: Can pause/revert if issues arise

### 4.2 Hybrid Build Setup

```
Project Structure (Phase 1-N):
├── src/                   (C++ main project)
│   ├── neworld.ixx
│   ├── ...
│   └── ffi/              (C++ FFI interface)
│       └── rust_bindings.cpp
├── rust/                 (Rust subproject)
│   ├── Cargo.toml
│   ├── src/
│   │   ├── lib.rs
│   │   ├── math/        (Migrated modules)
│   │   └── ffi/         (FFI exports)
│   └── build.rs         (Build script)
└── CMakeLists.txt       (Integrates Cargo build)
```

**Build Flow**:
1. CMake invokes `cargo build`
2. Rust generates static library `libneworld_rust.a`
3. C++ links against Rust static library
4. C++ calls Rust functions via FFI

### 4.3 FFI Boundary Design

**Example: Rust → C++ Interop**

```rust
// Rust side (rust/src/ffi/math.rs)
#[no_mangle]
pub extern "C" fn rust_vec3_add(
    a: *const Vec3, 
    b: *const Vec3, 
    out: *mut Vec3
) {
    unsafe {
        *out = (*a) + (*b);
    }
}
```

```cpp
// C++ side (src/ffi/rust_bindings.h)
extern "C" {
    void rust_vec3_add(const Vec3* a, const Vec3* b, Vec3* out);
}
```

**Recommended Tools**:
- `cbindgen`: Auto-generate C/C++ headers
- `cxx` crate: Safe C++/Rust interop (requires C++17+)

---

## 5. Phased Migration Plan

### Phase 0: Setup (2-3 weeks)

**Objective**: Establish Rust infrastructure

- [ ] Install Rust toolchain (rustc, cargo, clippy, rustfmt)
- [ ] Create Rust subproject (`rust/` directory)
- [ ] Integrate Cargo into CMake build
- [ ] Setup CI/CD (GitHub Actions)
- [ ] Write FFI template and best practices doc
- [ ] Team Rust training (basics + ownership)

**Validation**: Successfully call a simple Rust "Hello World" from C++

---

### Phase 1: Math Library (2-3 weeks)

**Objective**: Migrate `math` module (simplest, no external dependencies)

- [ ] Migrate `math/vector.ixx` → `rust/src/math/vector.rs`
- [ ] Migrate `math/matrix.ixx` → `rust/src/math/matrix.rs`
- [ ] Migrate `math/euler.ixx` → `rust/src/math/euler.rs`
- [ ] Migrate `math/aabb.ixx` → `rust/src/math/aabb.rs`
- [ ] Migrate `math/frustum.ixx` → `rust/src/math/frustum.rs`
- [ ] Use `glam` crate (high-performance SIMD vectors)
- [ ] Write FFI bridge layer
- [ ] Unit tests (compare with C++ results)

**Tech Stack**:
```toml
[dependencies]
glam = "0.29"  # SIMD-optimized game math
```

**Benefits**:
- ✅ Learn Rust basics and FFI
- ✅ Establish testing workflow
- ✅ Performance gains (SIMD optimization)

---

### Phase 2: Basic Types & Utilities (2 weeks)

**Objective**: Migrate common utilities

- [ ] Migrate `types.ixx` → `rust/src/types.rs`
- [ ] Migrate `globalization.ixx` → `rust/src/i18n.rs`
- [ ] Setup logging (`tracing` crate)
- [ ] UTF-8 string handling (Rust native)

---

### Phase 3: Block & Chunk Systems (3-4 weeks)

**Objective**: Migrate core data structures

- [ ] Migrate `blocks.ixx` → `rust/src/blocks.rs`
- [ ] Migrate `chunks.ixx` → `rust/src/chunks.rs`
- [ ] Migrate `chunk_pointer_arrays.ixx` → `rust/src/chunk_ptr.rs`
- [ ] Migrate `height_maps.ixx` → `rust/src/height_map.rs`
- [ ] Optimize memory layout (Rust `#[repr(C)]`)

**Key Challenges**:
- Efficient 16³ block array representation
- Light data bit operations
- Chunk ID encoding (28-8-28 bit)

**Rust Optimization**:
```rust
#[derive(Copy, Clone, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct BlockId(u16);

#[repr(C)]
pub struct Chunk {
    blocks: Box<[BlockId; 4096]>,  // 16³ = 4096
    light: Box<[u8; 4096]>,
}
```

---

### Phase 4: Terrain Generation (2-3 weeks)

**Objective**: Migrate noise generation and terrain algorithms

- [ ] Migrate `terrain_generation.ixx` → `rust/src/terrain_gen.rs`
- [ ] Use `noise` crate (Perlin/Simplex noise)
- [ ] Optimize with parallel generation (Rayon)

**Tech Stack**:
```toml
[dependencies]
noise = "0.9"
rayon = "1.10"  # Data parallelism
```

---

### Phase 5: Rendering Abstraction Layer (4-5 weeks)

**Objective**: Migrate OpenGL abstractions

- [ ] Migrate `render/buffer.ixx` → `rust/src/render/buffer.rs`
- [ ] Migrate `render/texture.ixx` → `rust/src/render/texture.rs`
- [ ] Migrate `render/framebuffer.ixx` → `rust/src/render/framebuffer.rs`
- [ ] Migrate `render/vertex_array.ixx` → `rust/src/render/vao.rs`
- [ ] Migrate `render/program.ixx` → `rust/src/render/shader.rs`

**RAII Pattern**:
```rust
pub struct Buffer {
    id: u32,
}

impl Drop for Buffer {
    fn drop(&mut self) {
        unsafe { gl::DeleteBuffers(1, &self.id) };
    }
}
```

---

### Phase 6-14: Remaining Modules

**Abbreviated**:
- **Phase 6**: Mesh builders (3 weeks)
- **Phase 7**: World persistence & LevelDB (3-4 weeks)
- **Phase 8**: Text rendering (2 weeks)
- **Phase 9**: UI system (5-6 weeks) - Most complex!
- **Phase 10**: Game logic (4-5 weeks)
- **Phase 11**: Menu system (3 weeks)
- **Phase 12**: Rendering pipeline (5-6 weeks)
- **Phase 13**: Main loop & window (3-4 weeks)
- **Phase 14**: Optimization & cleanup (2-3 weeks)

---

## 6. Technical Challenges

### 6.1 OpenGL Lifecycle Management

**Challenge**: RAII for OpenGL objects

**Solution**:
- All OpenGL resources created/destroyed on main render thread
- Use `Arc` for shared resource references
- Consider `glow` crate (safer OpenGL wrapper)

### 6.2 Async I/O Integration

**Challenge**: Integrate LevelDB async I/O into game loop

**Solution**:
```rust
pub struct WorldLoader {
    db: Arc<leveldb::Database>,
    runtime: tokio::runtime::Runtime,
}

impl WorldLoader {
    pub fn load_chunk_async(&self, pos: ChunkPos) -> JoinHandle<Result<Chunk>> {
        let db = self.db.clone();
        self.runtime.spawn(async move {
            // async load...
        })
    }
}
```

### 6.3 UI Framework Migration

**Challenge**: Complex constraint layout system

**Alternatives**:
1. **Direct port**: Keep original design, use trait objects
2. **Use existing library**: `egui` (immediate mode)
3. **Simplify**: Reduce abstraction layers

**Recommendation**: Evaluate `egui` integration before Phase 9

---

## 7. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| **Team learning curve** | High | Medium | Early training, gradual migration |
| **FFI performance overhead** | Medium | Medium | Benchmarking, optimize boundaries |
| **Library compatibility** | Low | High | Validate early (Phase 0) |
| **UI migration complexity** | High | High | Consider mature libraries (egui) |
| **Timeline overrun** | Medium | High | Buffer time, can pause |

---

## 8. Resource Estimation

### 8.1 Timeline

| Phase | Weeks | Cumulative |
|-------|-------|-----------|
| Phase 0: Setup | 2-3 | 3 |
| Phase 1-4: Core systems | 9-13 | 16 |
| Phase 5-8: Rendering | 11-14 | 30 |
| Phase 9-11: UI & game logic | 12-14 | 44 |
| Phase 12-14: Pipeline & main loop | 10-12 | 56 |

**Total**: **~56 weeks (13-14 months)**

### 8.2 Staffing

- **1 full-time developer**: ~14 months
- **2 full-time developers**: ~7-8 months (parallel phases)
- **Part-time (50%)**: ~28 months

---

## 9. Recommendations

### 9.1 Minimum Viable Migration (MVP)

If resources are limited, prioritize:

1. **Phase 1**: Math library (high value, low risk)
2. **Phase 3**: Block/Chunk systems (high memory safety gains)
3. **Phase 4**: Terrain generation (easy to parallelize)
4. **Phase 7**: World persistence (data safety critical)

**Timeline**: ~3-4 months

### 9.2 Full Migration Roadmap

Follow **Phase 0-14** in sequence: **~13-14 months**

### 9.3 Hybrid Approach (Long-term Coexistence)

If full migration is too risky:
- Core data structures and logic in Rust (Phase 1-7, 10)
- Keep UI in C++ (or use imgui-rs)
- Rendering layer optional (Phase 5-6, 12)

**Advantage**: Reduced risk, maintain flexibility

### 9.4 Modern Alternative: Bevy Engine

**Consider migrating to Bevy**:
- Bevy is a modern Rust game engine
- Provides ECS, rendering, UI, and more
- Active community, rich ecosystem

**Migration difficulty**: Requires architectural redesign, but greater long-term benefits

---

## Appendices

### A. Learning Resources

- **The Rust Book**: https://doc.rust-lang.org/book/
- **Rust by Example**: https://doc.rust-lang.org/rust-by-example/
- **Game Development**: https://arewegameyet.rs/
- **FFI Guide**: https://doc.rust-lang.org/nomicon/ffi.html

### B. Toolchain Setup

```bash
# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Essential components
rustup component add clippy rustfmt
cargo install cargo-audit cargo-edit cbindgen

# Performance analysis
cargo install flamegraph cargo-instruments
```

### C. Cargo.toml Template

```toml
[package]
name = "neworld"
version = "0.5.0"
edition = "2021"
rust-version = "1.75"

[dependencies]
gl = "0.14"
glam = "0.29"
winit = "0.30"
glutin = "0.32"
freetype = "0.7"
png = "0.17"
leveldb = "0.8"
tokio = { version = "1", features = ["full"] }
serde = { version = "1", features = ["derive"] }
bincode = "1.3"
tracing = "0.1"
tracing-subscriber = "0.3"
noise = "0.9"
rayon = "1.10"

[profile.release]
lto = true
codegen-units = 1
opt-level = 3
```

---

**Document End**

For questions or detailed implementation plans for specific phases, please contact the project maintainers.
