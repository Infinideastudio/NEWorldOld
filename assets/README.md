# Assets

Runtime assets used by the Rust port. Layout:

| Directory | Contents |
|-----------|----------|
| `shaders/` | WGSL shader sources, ported from the GLSL files in the repo's top-level `shaders/`. |
| `textures/` | Block atlases, UI textures, splash images. Mirrors the layout of the repo's top-level `textures/`. |
| `fonts/` | TrueType fonts for HUD and menu text. |
| `lang/` | Localisation tables. Format and key list are owned by the `i18n` module. |

The C++ build resolves these paths from the working directory at runtime
(`shaders/`, `textures/`, etc.). The Rust port resolves them under `rs/assets/`
relative to the binary's working directory; copy or symlink the repo's
top-level asset folders here when running the binary directly.
