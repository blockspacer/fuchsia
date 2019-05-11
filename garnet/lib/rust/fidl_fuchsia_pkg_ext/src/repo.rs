// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    crate::errors::RepositoryParseError,
    fidl_fuchsia_pkg as fidl,
    fuchsia_uri::pkg_uri::{PkgUri, RepoUri},
    serde_derive::{Deserialize, Serialize},
    std::convert::TryFrom,
    std::mem,
};

/// Convenience wrapper for the FIDL RepositoryKeyConfig type
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase", tag = "type", content = "value", deny_unknown_fields)]
pub enum RepositoryKey {
    Ed25519(#[serde(with = "hex_serde")] Vec<u8>),
}

/// Convenience wrapper for the FIDL RepositoryBlobConfig type
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase", tag = "type", content = "value", deny_unknown_fields)]
pub enum RepositoryBlobKey {
    Aes(#[serde(with = "hex_serde")] Vec<u8>),
}

/// Convenience wrapper for the FIDL MirrorConfig type
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct MirrorConfig {
    mirror_url: String,
    subscribe: bool,
    blob_key: Option<RepositoryBlobKey>,
}

impl MirrorConfig {
    pub fn mirror_url(&self) -> &str {
        &self.mirror_url
    }
}

/// Convenience wrapper for generating [MirrorConfig] values.
pub struct MirrorConfigBuilder {
    config: MirrorConfig,
}

impl MirrorConfigBuilder {
    pub fn new(mirror_url: String) -> Self {
        MirrorConfigBuilder {
            config: MirrorConfig { mirror_url: mirror_url, subscribe: false, blob_key: None },
        }
    }

    pub fn subscribe(mut self, subscribe: bool) -> Self {
        self.config.subscribe = subscribe;
        self
    }

    pub fn blob_key(mut self, blob_key: RepositoryBlobKey) -> Self {
        self.config.blob_key = Some(blob_key);
        self
    }

    pub fn build(self) -> MirrorConfig {
        self.config
    }
}

impl TryFrom<fidl::MirrorConfig> for MirrorConfig {
    type Error = RepositoryParseError;
    fn try_from(other: fidl::MirrorConfig) -> Result<Self, RepositoryParseError> {
        Ok(Self {
            mirror_url: other.mirror_url.ok_or(RepositoryParseError::MirrorUrlMissing)?,
            subscribe: other.subscribe.ok_or(RepositoryParseError::SubscribeMissing)?,
            blob_key: other.blob_key.map(RepositoryBlobKey::try_from).transpose()?,
        })
    }
}

impl Into<fidl::MirrorConfig> for MirrorConfig {
    fn into(self: Self) -> fidl::MirrorConfig {
        fidl::MirrorConfig {
            mirror_url: Some(self.mirror_url),
            subscribe: Some(self.subscribe),
            blob_key: self.blob_key.map(|k| k.into()),
        }
    }
}

/// Convenience wrapper type for the autogenerated FIDL `RepositoryConfig`.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct RepositoryConfig {
    repo_url: RepoUri,
    root_keys: Vec<RepositoryKey>,
    mirrors: Vec<MirrorConfig>,
    update_package_uri: Option<PkgUri>,
}

impl RepositoryConfig {
    pub fn repo_url(&self) -> &RepoUri {
        &self.repo_url
    }

    pub fn insert_mirror(&mut self, mut mirror: MirrorConfig) -> Option<MirrorConfig> {
        if let Some(m) = self.mirrors.iter_mut().find(|m| m.mirror_url == mirror.mirror_url) {
            mem::swap(m, &mut mirror);
            Some(mirror)
        } else {
            self.mirrors.push(mirror);
            None
        }
    }

    pub fn remove_mirror(&mut self, mirror_url: &str) -> Option<MirrorConfig> {
        if let Some(pos) = self.mirrors.iter().position(|m| m.mirror_url == mirror_url) {
            Some(self.mirrors.remove(pos))
        } else {
            None
        }
    }
}

impl TryFrom<fidl::RepositoryConfig> for RepositoryConfig {
    type Error = RepositoryParseError;

    fn try_from(other: fidl::RepositoryConfig) -> Result<Self, RepositoryParseError> {
        let repo_url: RepoUri = other
            .repo_url
            .ok_or(RepositoryParseError::RepoUrlMissing)?
            .parse()
            .map_err(|err| RepositoryParseError::InvalidRepoUrl(err))?;

        let update_package_uri = if let Some(uri) = other.update_package_uri {
            let uri =
                uri.parse().map_err(|err| RepositoryParseError::InvalidUpdatePackageUri(err))?;
            Some(uri)
        } else {
            None
        };

        Ok(Self {
            repo_url: repo_url,
            root_keys: other
                .root_keys
                .unwrap_or(vec![])
                .into_iter()
                .map(RepositoryKey::try_from)
                .collect::<Result<_, _>>()?,
            mirrors: other
                .mirrors
                .unwrap_or(vec![])
                .into_iter()
                .map(MirrorConfig::try_from)
                .collect::<Result<_, _>>()?,
            update_package_uri: update_package_uri,
        })
    }
}

impl Into<fidl::RepositoryConfig> for RepositoryConfig {
    fn into(self: Self) -> fidl::RepositoryConfig {
        fidl::RepositoryConfig {
            repo_url: Some(self.repo_url.to_string()),
            root_keys: Some(self.root_keys.into_iter().map(RepositoryKey::into).collect()),
            mirrors: Some(self.mirrors.into_iter().map(MirrorConfig::into).collect()),
            update_package_uri: self.update_package_uri.map(|uri| uri.to_string()),
        }
    }
}

/// Convenience wrapper for generating [RepositoryConfig] values.
pub struct RepositoryConfigBuilder {
    config: RepositoryConfig,
}

impl RepositoryConfigBuilder {
    pub fn new(repo_url: RepoUri) -> Self {
        RepositoryConfigBuilder {
            config: RepositoryConfig {
                repo_url: repo_url,
                root_keys: vec![],
                mirrors: vec![],
                update_package_uri: None,
            },
        }
    }

    pub fn add_root_key(mut self, key: RepositoryKey) -> Self {
        self.config.root_keys.push(key);
        self
    }

    pub fn add_mirror(mut self, mirror: MirrorConfig) -> Self {
        self.config.mirrors.push(mirror);
        self
    }

    pub fn update_package_uri(mut self, uri: PkgUri) -> Self {
        self.config.update_package_uri = Some(uri);
        self
    }

    pub fn build(self) -> RepositoryConfig {
        self.config
    }
}

/// Wraper for serializing repository configs to the on-disk JSON format.
#[derive(Clone, Debug, PartialEq, Eq, Deserialize, Serialize)]
#[serde(tag = "version", content = "content", deny_unknown_fields)]
pub enum RepositoryConfigs {
    #[serde(rename = "1")]
    Version1(Vec<RepositoryConfig>),
}

impl TryFrom<fidl::RepositoryKeyConfig> for RepositoryKey {
    type Error = RepositoryParseError;
    fn try_from(id: fidl::RepositoryKeyConfig) -> Result<Self, RepositoryParseError> {
        match id {
            fidl::RepositoryKeyConfig::Ed25519Key(key) => Ok(RepositoryKey::Ed25519(key)),
            _ => Err(RepositoryParseError::UnsupportedKeyType),
        }
    }
}

impl Into<fidl::RepositoryKeyConfig> for RepositoryKey {
    fn into(self: Self) -> fidl::RepositoryKeyConfig {
        match self {
            RepositoryKey::Ed25519(key) => fidl::RepositoryKeyConfig::Ed25519Key(key),
        }
    }
}

impl TryFrom<fidl::RepositoryBlobKey> for RepositoryBlobKey {
    type Error = RepositoryParseError;
    fn try_from(id: fidl::RepositoryBlobKey) -> Result<Self, RepositoryParseError> {
        match id {
            fidl::RepositoryBlobKey::AesKey(key) => Ok(RepositoryBlobKey::Aes(key)),
            _ => Err(RepositoryParseError::UnsupportedKeyType),
        }
    }
}

impl Into<fidl::RepositoryBlobKey> for RepositoryBlobKey {
    fn into(self: Self) -> fidl::RepositoryBlobKey {
        match self {
            RepositoryBlobKey::Aes(key) => fidl::RepositoryBlobKey::AesKey(key),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::convert::TryInto;

    #[test]
    fn test_repository_key_serialize() {
        let key = RepositoryKey::Ed25519(vec![0xf1, 15, 16, 3]);
        assert_eq!(
            serde_json::to_string(&key).unwrap(),
            r#"{"type":"ed25519","value":"f10f1003"}"#
        );
    }

    #[test]
    fn test_repository_key_deserialize() {
        let json = r#"{"type":"ed25519","value":"00010203"}"#;
        assert_eq!(
            serde_json::from_str::<RepositoryKey>(json).unwrap(),
            RepositoryKey::Ed25519(vec![0, 1, 2, 3])
        );
    }

    #[test]
    fn test_repository_key_deserialize_with_bad_type() {
        let json = r#"{"type":"bogus","value":"00010203"}"#;
        let result = serde_json::from_str::<RepositoryKey>(json);
        let error_message = result.unwrap_err().to_string();
        assert!(
            error_message.contains("unknown variant `bogus`"),
            r#"Error message did not contain "unknown variant `bogus`", was "{}""#,
            error_message
        );
    }

    #[test]
    fn test_repository_key_deserialize_serialize_roundtrip() {
        let json = r#"{"type":"ed25519","value":"00010203"}"#;

        let key: RepositoryKey = serde_json::from_str(json).unwrap();
        assert_eq!(serde_json::to_string(&key).unwrap(), json);
    }

    #[test]
    fn test_repository_key_into_fidl() {
        let key = RepositoryKey::Ed25519(vec![0xf1, 15, 16, 3]);
        let as_fidl: fidl::RepositoryKeyConfig = key.into();
        assert_eq!(as_fidl, fidl::RepositoryKeyConfig::Ed25519Key(vec![0xf1, 15, 16, 3]))
    }

    #[test]
    fn test_repository_key_from_fidl() {
        let as_fidl = fidl::RepositoryKeyConfig::Ed25519Key(vec![0xf1, 15, 16, 3]);
        assert_eq!(
            RepositoryKey::try_from(as_fidl),
            Ok(RepositoryKey::Ed25519(vec![0xf1, 15, 16, 3]))
        );
    }

    #[test]
    fn test_repository_key_from_fidl_with_bad_type() {
        let as_fidl = fidl::RepositoryKeyConfig::__UnknownVariant {
            ordinal: 999,
            bytes: vec![],
            handles: vec![],
        };
        assert_eq!(RepositoryKey::try_from(as_fidl), Err(RepositoryParseError::UnsupportedKeyType));
    }

    #[test]
    fn test_repository_key_into_from_fidl_roundtrip() {
        let key = RepositoryKey::Ed25519(vec![0xf1, 15, 16, 3]);
        let as_fidl: fidl::RepositoryKeyConfig = key.clone().into();
        assert_eq!(RepositoryKey::try_from(as_fidl).unwrap(), key,)
    }

    #[test]
    fn test_blob_key_into_fidl() {
        let key = RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3]);
        let as_fidl: fidl::RepositoryBlobKey = key.into();
        assert_eq!(as_fidl, fidl::RepositoryBlobKey::AesKey(vec![0xf1, 15, 16, 3]))
    }

    #[test]
    fn test_blob_key_from_fidl() {
        let as_fidl = fidl::RepositoryBlobKey::AesKey(vec![0xf1, 15, 16, 3]);
        assert_eq!(
            RepositoryBlobKey::try_from(as_fidl),
            Ok(RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3]))
        );
    }

    #[test]
    fn test_blob_key_from_fidl_with_bad_type() {
        let as_fidl = fidl::RepositoryBlobKey::__UnknownVariant {
            ordinal: 999,
            bytes: vec![],
            handles: vec![],
        };
        assert_eq!(
            RepositoryBlobKey::try_from(as_fidl),
            Err(RepositoryParseError::UnsupportedKeyType)
        );
    }

    #[test]
    fn test_blob_key_into_from_fidl_roundtrip() {
        let key = RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3]);
        let as_fidl: fidl::RepositoryBlobKey = key.clone().into();
        assert_eq!(RepositoryBlobKey::try_from(as_fidl).unwrap(), key);
    }

    #[test]
    fn test_mirror_config_into_fidl() {
        let config = MirrorConfig {
            mirror_url: "http://example.com/tuf/repo".into(),
            subscribe: true,
            blob_key: Some(RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3])),
        };
        let as_fidl: fidl::MirrorConfig = config.into();
        assert_eq!(
            as_fidl,
            fidl::MirrorConfig {
                mirror_url: Some("http://example.com/tuf/repo".into()),
                subscribe: Some(true),
                blob_key: Some(fidl::RepositoryBlobKey::AesKey(vec![0xf1, 15, 16, 3])),
            }
        );
    }

    #[test]
    fn test_mirror_config_from_fidl() {
        let as_fidl = fidl::MirrorConfig {
            mirror_url: Some("http://example.com/tuf/repo".into()),
            subscribe: Some(true),
            blob_key: Some(fidl::RepositoryBlobKey::AesKey(vec![0xf1, 15, 16, 3])),
        };
        assert_eq!(
            MirrorConfig::try_from(as_fidl),
            Ok(MirrorConfig {
                mirror_url: "http://example.com/tuf/repo".into(),
                subscribe: true,
                blob_key: Some(RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3])),
            })
        );
    }

    #[test]
    fn test_mirror_config_into_from_fidl_roundtrip() {
        let config = MirrorConfig {
            mirror_url: "http://example.com/tuf/repo".into(),
            subscribe: true,
            blob_key: Some(RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3])),
        };
        let as_fidl: fidl::MirrorConfig = config.clone().into();
        assert_eq!(MirrorConfig::try_from(as_fidl).unwrap(), config);
    }

    #[test]
    fn test_repository_config_into_fidl() {
        let config = RepositoryConfig {
            repo_url: "fuchsia-pkg://fuchsia.com".try_into().unwrap(),
            root_keys: vec![RepositoryKey::Ed25519(vec![0xf1, 15, 16, 3])],
            mirrors: vec![MirrorConfig {
                mirror_url: "http://example.com/tuf/repo".into(),
                subscribe: true,
                blob_key: Some(RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3])),
            }],
            update_package_uri: Some("fuchsia-pkg://fuchsia.com/systemupdate".try_into().unwrap()),
        };
        let as_fidl: fidl::RepositoryConfig = config.into();
        assert_eq!(
            as_fidl,
            fidl::RepositoryConfig {
                repo_url: Some("fuchsia-pkg://fuchsia.com".try_into().unwrap()),
                root_keys: Some(vec![fidl::RepositoryKeyConfig::Ed25519Key(vec![0xf1, 15, 16, 3])]),
                mirrors: Some(vec![fidl::MirrorConfig {
                    mirror_url: Some("http://example.com/tuf/repo".into()),
                    subscribe: Some(true),
                    blob_key: Some(fidl::RepositoryBlobKey::AesKey(vec![0xf1, 15, 16, 3])),
                }]),
                update_package_uri: Some(
                    "fuchsia-pkg://fuchsia.com/systemupdate".try_into().unwrap()
                ),
            }
        );
    }

    #[test]
    fn test_repository_config_from_fidl() {
        let as_fidl = fidl::RepositoryConfig {
            repo_url: Some("fuchsia-pkg://fuchsia.com".try_into().unwrap()),
            root_keys: Some(vec![fidl::RepositoryKeyConfig::Ed25519Key(vec![0xf1, 15, 16, 3])]),
            mirrors: Some(vec![fidl::MirrorConfig {
                mirror_url: Some("http://example.com/tuf/repo".into()),
                subscribe: Some(true),
                blob_key: Some(fidl::RepositoryBlobKey::AesKey(vec![0xf1, 15, 16, 3])),
            }]),
            update_package_uri: Some("fuchsia-pkg://fuchsia.com/systemupdate".try_into().unwrap()),
        };
        assert_eq!(
            RepositoryConfig::try_from(as_fidl),
            Ok(RepositoryConfig {
                repo_url: "fuchsia-pkg://fuchsia.com".try_into().unwrap(),
                root_keys: vec![RepositoryKey::Ed25519(vec![0xf1, 15, 16, 3]),],
                mirrors: vec![MirrorConfig {
                    mirror_url: "http://example.com/tuf/repo".into(),
                    subscribe: true,
                    blob_key: Some(RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3])),
                },],
                update_package_uri: Some(
                    "fuchsia-pkg://fuchsia.com/systemupdate".try_into().unwrap()
                ),
            })
        );
    }

    #[test]
    fn test_repository_config_from_fidl_repo_url_missing() {
        let as_fidl = fidl::RepositoryConfig {
            repo_url: None,
            root_keys: Some(vec![]),
            mirrors: Some(vec![]),
            update_package_uri: Some("fuchsia-pkg://fuchsia.com/systemupdate".try_into().unwrap()),
        };
        assert_eq!(RepositoryConfig::try_from(as_fidl), Err(RepositoryParseError::RepoUrlMissing));
    }

    #[test]
    fn test_repository_config_into_from_fidl_roundtrip() {
        let config = RepositoryConfig {
            repo_url: "fuchsia-pkg://fuchsia.com".try_into().unwrap(),
            root_keys: vec![RepositoryKey::Ed25519(vec![0xf1, 15, 16, 3])],
            mirrors: vec![MirrorConfig {
                mirror_url: "http://example.com/tuf/repo".into(),
                subscribe: true,
                blob_key: Some(RepositoryBlobKey::Aes(vec![0xf1, 15, 16, 3])),
            }],
            update_package_uri: Some("fuchsia-pkg://fuchsia.com/systemupdate".try_into().unwrap()),
        };
        let as_fidl: fidl::RepositoryConfig = config.clone().into();
        assert_eq!(RepositoryConfig::try_from(as_fidl).unwrap(), config);
    }

    #[test]
    fn test_repository_configs_serialize() {
        let configs = RepositoryConfigs::Version1(vec![RepositoryConfig {
            repo_url: "fuchsia-pkg://fuchsia.com".try_into().unwrap(),
            root_keys: vec![],
            mirrors: vec![],
            update_package_uri: None,
        }]);
        assert_eq!(
            serde_json::to_string(&configs).unwrap(),
            r#"{"version":"1","content":[{"repo_url":"fuchsia-pkg://fuchsia.com","root_keys":[],"mirrors":[],"update_package_uri":null}]}"#
        )
    }
}

mod hex_serde {
    use {hex, serde::Deserialize};

    pub fn serialize<S>(bytes: &[u8], serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        let s = hex::encode(bytes);
        serializer.serialize_str(&s)
    }

    pub fn deserialize<'de, D>(deserializer: D) -> Result<Vec<u8>, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let value = String::deserialize(deserializer)?;
        hex::decode(value.as_bytes())
            .map_err(|e| serde::de::Error::custom(format!("bad hex value: {:?}: {}", value, e)))
    }
}
