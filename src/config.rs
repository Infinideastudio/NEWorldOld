//! Game options, loaded from / saved to a TOML file.
//!
//! Replaces the C++ `globals.ixx` `load_options`/`save_options` pair. The on-disk
//! format is TOML (the migration plan mandates this switch from the legacy INI),
//! and there is no backward compatibility with the C++ build's `options.ini`.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Default location of the options file, relative to the working directory.
pub const DEFAULT_PATH: &str = "configs/options.toml";

/// All user-tweakable options. Live-edited by the in-game options menu.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
#[serde(default)]
#[allow(clippy::struct_excessive_bools)] // mirrors the C++ option set verbatim
pub struct Config {
    pub language: String,
    pub fov_y_normal: f32,
    pub mouse_speed: f32,
    pub render_distance: i32,
    pub smooth_lighting: bool,
    pub nice_grass: bool,
    pub merge_face: bool,
    pub multisample: i32,
    pub advanced_render: bool,
    pub shadow_res: i32,
    pub max_shadow_distance: i32,
    pub soft_shadow: bool,
    pub volumetric_clouds: bool,
    pub ambient_occlusion: bool,
    pub vertical_sync: bool,
    pub font_scale: f64,
    pub ui_auto_stretch: bool,
    pub ui_background_blur: bool,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            language: "zh_CN".to_owned(),
            fov_y_normal: 70.0,
            mouse_speed: 0.1,
            render_distance: 16,
            smooth_lighting: true,
            nice_grass: true,
            merge_face: false,
            multisample: 4,
            advanced_render: false,
            shadow_res: 2048,
            max_shadow_distance: 16,
            soft_shadow: false,
            volumetric_clouds: false,
            ambient_occlusion: false,
            vertical_sync: false,
            font_scale: 1.0,
            ui_auto_stretch: true,
            ui_background_blur: true,
        }
    }
}

/// Errors produced by `Config::load_from` / `Config::save_to`.
///
/// The TOML error variants are boxed so the enum stays small in the common
/// success path (avoids clippy's `result_large_err`).
#[derive(Debug, Error)]
pub enum ConfigError {
    #[error("config I/O error at {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: io::Error,
    },
    #[error("config parse error at {path}: {source}")]
    Parse {
        path: PathBuf,
        #[source]
        source: Box<toml::de::Error>,
    },
    #[error("config serialize error: {0}")]
    Serialize(Box<toml::ser::Error>),
}

impl From<toml::ser::Error> for ConfigError {
    fn from(e: toml::ser::Error) -> Self {
        Self::Serialize(Box::new(e))
    }
}

impl Config {
    /// Load the config from `path`. Returns `Config::default()` if the file does
    /// not exist (so first launches just work). Malformed files surface an error
    /// rather than being silently overwritten.
    pub fn load_from(path: &Path) -> Result<Self, ConfigError> {
        let text = match fs::read_to_string(path) {
            Ok(t) => t,
            Err(e) if e.kind() == io::ErrorKind::NotFound => return Ok(Self::default()),
            Err(source) => {
                return Err(ConfigError::Io {
                    path: path.to_path_buf(),
                    source,
                });
            }
        };
        toml::from_str(&text).map_err(|source| ConfigError::Parse {
            path: path.to_path_buf(),
            source: Box::new(source),
        })
    }

    /// Serialize and atomically write the config to `path`. Creates the parent
    /// directory if necessary; writes to `<path>.tmp` then renames on top.
    pub fn save_to(&self, path: &Path) -> Result<(), ConfigError> {
        let text = toml::to_string_pretty(self)?;
        if let Some(parent) = path.parent()
            && !parent.as_os_str().is_empty()
        {
            fs::create_dir_all(parent).map_err(|source| ConfigError::Io {
                path: parent.to_path_buf(),
                source,
            })?;
        }
        let mut tmp = path.as_os_str().to_owned();
        tmp.push(".tmp");
        let tmp = PathBuf::from(tmp);
        fs::write(&tmp, text).map_err(|source| ConfigError::Io {
            path: tmp.clone(),
            source,
        })?;
        fs::rename(&tmp, path).map_err(|source| ConfigError::Io {
            path: path.to_path_buf(),
            source,
        })?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn temp_dir(label: &str) -> PathBuf {
        let mut p = std::env::temp_dir();
        let nanos = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_or(0, |d| d.as_nanos());
        p.push(format!(
            "neworld-config-test-{label}-{}-{nanos}",
            std::process::id()
        ));
        fs::create_dir_all(&p).expect("create temp dir");
        p
    }

    #[test]
    fn default_round_trips_through_toml() {
        let dir = temp_dir("roundtrip");
        let path = dir.join("options.toml");
        let original = Config::default();
        original.save_to(&path).expect("save");
        let loaded = Config::load_from(&path).expect("load");
        assert_eq!(original, loaded);
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn load_returns_default_when_file_missing() {
        let dir = temp_dir("missing");
        let path = dir.join("does-not-exist.toml");
        let loaded = Config::load_from(&path).expect("load missing");
        assert_eq!(loaded, Config::default());
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn save_creates_parent_dirs() {
        let dir = temp_dir("parents");
        let path = dir.join("nested/deeper/options.toml");
        assert!(!path.parent().unwrap().exists());
        Config::default().save_to(&path).expect("save");
        assert!(path.exists(), "config file written");
        assert!(path.parent().unwrap().is_dir(), "parent dir created");
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn malformed_file_surfaces_parse_error() {
        let dir = temp_dir("malformed");
        let path = dir.join("options.toml");
        fs::write(&path, "this is = = not valid toml [[[").expect("seed file");
        let err = Config::load_from(&path).expect_err("expected parse error");
        assert!(matches!(err, ConfigError::Parse { .. }));
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn partial_file_uses_defaults_for_missing_fields() {
        let dir = temp_dir("partial");
        let path = dir.join("options.toml");
        fs::write(&path, "render_distance = 8\nlanguage = \"en_US\"\n").expect("seed file");
        let loaded = Config::load_from(&path).expect("load partial");
        assert_eq!(loaded.render_distance, 8);
        assert_eq!(loaded.language, "en_US");
        // Untouched fields fall back to defaults; bit-equal so no float-cmp warning.
        assert_eq!(
            loaded.fov_y_normal.to_bits(),
            Config::default().fov_y_normal.to_bits()
        );
        assert_eq!(
            loaded.ui_background_blur,
            Config::default().ui_background_blur
        );
        let _ = fs::remove_dir_all(&dir);
    }
}
