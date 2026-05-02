//! Localisation — string tables loaded from `assets/lang/`.
//!
//! Per the Rust migration plan (§4.8), the language tables are re-authored as
//! one TOML file per language with a flat `key = "value"` table. There is no
//! backward compatibility with the old C++ `lang/keys.lk` + `lang/*.lang`
//! two-pass format.
//!
//! Each language file lives at `<lang_dir>/<code>.toml`, e.g.
//! `assets/lang/en_US.toml`:
//!
//! ```toml
//! "NEWorld.yes" = "Yes"
//! "NEWorld.no"  = "No"
//! ```
//!
//! `I18n::get` returns `""` for a missing key rather than inserting into the
//! map — `std::map::operator[]`'s side effect was a footgun in the C++
//! version and we don't want to replicate it.

use std::collections::HashMap;
use std::path::{Path, PathBuf};

use thiserror::Error;

/// A loaded language table.
#[derive(Debug, Clone)]
pub struct I18n {
    current: String,
    lines: HashMap<String, String>,
}

/// Errors returned by [`I18n::load`] / [`I18n::reload`].
#[derive(Debug, Error)]
pub enum I18nError {
    /// I/O failure reading the language file (e.g. file not found).
    #[error("failed to read language file {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },
    /// The language file was not valid TOML, or contained a non-string value.
    #[error("failed to parse language file {path}: {source}")]
    Parse {
        path: PathBuf,
        // `toml::de::Error` is ~128 bytes; box it to keep `Result<_, I18nError>`
        // small (clippy::result_large_err).
        #[source]
        source: Box<toml::de::Error>,
    },
}

impl I18n {
    /// Build an empty table tagged with `lang_code`. Used as a fallback when
    /// the configured language file is missing or malformed at boot time —
    /// every `get(_)` returns `""`, so the menu shows blank captions instead
    /// of crashing the app.
    pub fn empty(lang_code: &str) -> Self {
        Self {
            current: lang_code.to_owned(),
            lines: HashMap::new(),
        }
    }

    /// Load the language file `<lang_dir>/<lang_code>.toml`.
    pub fn load(lang_code: &str, lang_dir: &Path) -> Result<Self, I18nError> {
        let path = Self::path_for(lang_code, lang_dir);
        let text = std::fs::read_to_string(&path).map_err(|source| I18nError::Io {
            path: path.clone(),
            source,
        })?;
        let lines: HashMap<String, String> =
            toml::from_str(&text).map_err(|source| I18nError::Parse {
                path: path.clone(),
                source: Box::new(source),
            })?;
        Ok(Self {
            current: lang_code.to_owned(),
            lines,
        })
    }

    /// Reload `self` from `<lang_dir>/<lang_code>.toml`. On error `self` is
    /// left unchanged.
    pub fn reload(&mut self, lang_code: &str, lang_dir: &Path) -> Result<(), I18nError> {
        let next = Self::load(lang_code, lang_dir)?;
        *self = next;
        Ok(())
    }

    /// Look up a translated string by key. Returns `""` if the key is not
    /// present (the map is **not** mutated).
    pub fn get(&self, key: &str) -> &str {
        self.lines.get(key).map_or("", String::as_str)
    }

    /// The current language code (e.g. `"en_US"`).
    pub fn current(&self) -> &str {
        &self.current
    }

    fn path_for(lang_code: &str, lang_dir: &Path) -> PathBuf {
        lang_dir.join(format!("{lang_code}.toml"))
    }
}

/// One entry returned by [`list_languages`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LanguageEntry {
    /// Filename stem, e.g. `"en_US"`. Use as the `lang_code` argument to
    /// [`I18n::load`] / [`I18n::reload`].
    pub code: String,
    /// English name read from the `language.english_name` key in the file.
    /// Empty if the key was missing.
    pub english_name: String,
    /// Native name read from the `language.native_name` key. Empty if the
    /// key was missing. The language menu picks this for the button label.
    pub native_name: String,
}

/// Enumerate every `*.toml` under `lang_dir` and return one [`LanguageEntry`]
/// per file. Mirrors the C++ `language_menu.cpp` flow that reads
/// `lang/langs.txt` then opens each file for the first two lines (English +
/// native name); here we have a single TOML per language and pull the names
/// from the `language.english_name` / `language.native_name` keys.
///
/// Files that fail to read or parse are silently skipped — the language
/// picker shouldn't crash the menu just because one TOML is malformed.
/// Returns the list sorted by `code` for stable ordering across runs.
pub fn list_languages(lang_dir: &Path) -> Vec<LanguageEntry> {
    let mut out = Vec::new();
    let Ok(reader) = std::fs::read_dir(lang_dir) else {
        return out;
    };
    for entry in reader.flatten() {
        let path = entry.path();
        if path.extension().and_then(|s| s.to_str()) != Some("toml") {
            continue;
        }
        let Some(code) = path.file_stem().and_then(|s| s.to_str()) else {
            continue;
        };
        let code = code.to_owned();
        let Ok(text) = std::fs::read_to_string(&path) else {
            continue;
        };
        let parsed: Result<HashMap<String, String>, _> = toml::from_str(&text);
        let Ok(map) = parsed else {
            continue;
        };
        out.push(LanguageEntry {
            english_name: map
                .get("language.english_name")
                .cloned()
                .unwrap_or_default(),
            native_name: map.get("language.native_name").cloned().unwrap_or_default(),
            code,
        });
    }
    out.sort_by(|a, b| a.code.cmp(&b.code));
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::sync::atomic::{AtomicU64, Ordering};

    /// Minimal scratch directory under `std::env::temp_dir()`. We don't take
    /// a `tempfile` dependency for these tests (it's not in `Cargo.toml`);
    /// instead each test gets a uniquely-named subdirectory and `Drop` cleans
    /// it up.
    struct ScratchDir {
        path: PathBuf,
    }

    impl ScratchDir {
        fn new(tag: &str) -> Self {
            static COUNTER: AtomicU64 = AtomicU64::new(0);
            let n = COUNTER.fetch_add(1, Ordering::Relaxed);
            let pid = std::process::id();
            let path = std::env::temp_dir().join(format!("neworld-i18n-{tag}-{pid}-{n}"));
            // Best-effort cleanup of any prior leftover.
            let _ = fs::remove_dir_all(&path);
            fs::create_dir_all(&path).expect("create scratch dir");
            Self { path }
        }

        fn path(&self) -> &Path {
            &self.path
        }

        fn write(&self, name: &str, contents: &str) -> PathBuf {
            let file = self.path.join(name);
            fs::write(&file, contents).expect("write scratch file");
            file
        }
    }

    impl Drop for ScratchDir {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.path);
        }
    }

    const SAMPLE_EN_US: &str = r#"
"NEWorld.yes" = "Yes"
"NEWorld.no"  = "No"
"NEWorld.main.start" = "Start game"
"#;

    #[test]
    fn load_succeeds_on_valid_file() {
        let dir = ScratchDir::new("load-ok");
        dir.write("en_US.toml", SAMPLE_EN_US);

        let i18n = I18n::load("en_US", dir.path()).expect("load should succeed");
        assert_eq!(i18n.current(), "en_US");
    }

    #[test]
    fn get_returns_loaded_value_for_known_key() {
        let dir = ScratchDir::new("get-known");
        dir.write("en_US.toml", SAMPLE_EN_US);

        let i18n = I18n::load("en_US", dir.path()).expect("load");
        assert_eq!(i18n.get("NEWorld.yes"), "Yes");
        assert_eq!(i18n.get("NEWorld.no"), "No");
        assert_eq!(i18n.get("NEWorld.main.start"), "Start game");
    }

    #[test]
    fn get_returns_empty_for_unknown_key_without_inserting() {
        let dir = ScratchDir::new("get-missing");
        dir.write("en_US.toml", SAMPLE_EN_US);

        let i18n = I18n::load("en_US", dir.path()).expect("load");
        let before = i18n.lines.len();
        assert_eq!(i18n.get("NEWorld.does.not.exist"), "");
        // Per the migration plan: `get` must NOT insert on miss.
        assert_eq!(i18n.lines.len(), before);
    }

    #[test]
    fn load_errors_on_malformed_toml() {
        let dir = ScratchDir::new("load-bad");
        dir.write("en_US.toml", "this is = = not valid TOML\n[[[");

        let err = I18n::load("en_US", dir.path()).expect_err("malformed TOML must error");
        assert!(matches!(err, I18nError::Parse { .. }), "got {err:?}");
    }

    #[test]
    fn load_errors_when_file_missing() {
        let dir = ScratchDir::new("load-missing");
        // Note: deliberately do NOT write the file.

        let err = I18n::load("en_US", dir.path()).expect_err("missing file must error");
        assert!(matches!(err, I18nError::Io { .. }), "got {err:?}");
    }

    #[test]
    fn reload_replaces_current_language() {
        let dir = ScratchDir::new("reload");
        dir.write("en_US.toml", SAMPLE_EN_US);
        dir.write(
            "zh_CN.toml",
            "\"NEWorld.yes\" = \"\u{662f}\"\n\"NEWorld.no\" = \"\u{5426}\"\n",
        );

        let mut i18n = I18n::load("en_US", dir.path()).expect("load en_US");
        assert_eq!(i18n.current(), "en_US");
        assert_eq!(i18n.get("NEWorld.yes"), "Yes");

        i18n.reload("zh_CN", dir.path()).expect("reload zh_CN");
        assert_eq!(i18n.current(), "zh_CN");
        assert_eq!(i18n.get("NEWorld.yes"), "\u{662f}");
    }

    #[test]
    fn reload_leaves_state_unchanged_on_error() {
        let dir = ScratchDir::new("reload-err");
        dir.write("en_US.toml", SAMPLE_EN_US);

        let mut i18n = I18n::load("en_US", dir.path()).expect("load");
        let err = i18n
            .reload("missing", dir.path())
            .expect_err("missing target must error");
        assert!(matches!(err, I18nError::Io { .. }), "got {err:?}");
        // State preserved.
        assert_eq!(i18n.current(), "en_US");
        assert_eq!(i18n.get("NEWorld.yes"), "Yes");
    }
}
