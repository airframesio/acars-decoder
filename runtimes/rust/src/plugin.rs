//! Core plugin trait + message/result types.

use serde::Serialize;
use serde_json::Value as JsonValue;
use std::collections::HashMap;

/// Runtime value attached to `DecodeResult.raw`. Backed by `serde_json::Value`
/// so plugins can stash arbitrary nested data without bespoke types.
pub type RawValue = JsonValue;

#[derive(Debug, Clone, Serialize)]
pub struct Message {
    pub label: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub sublabel: Option<String>,
    pub text: String,
}

#[derive(Debug, Clone, Default, Serialize)]
pub struct Options {
    #[serde(default)]
    pub debug: bool,
}

#[derive(Debug, Clone, Serialize)]
pub struct Qualifiers {
    pub labels: Vec<&'static str>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub preambles: Vec<&'static str>,
}

#[derive(Debug, Clone, Serialize)]
pub struct DecoderInfo {
    pub name: &'static str,
    pub kind: &'static str,
    pub decode_level: DecodeLevel,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum DecodeLevel {
    None,
    Partial,
    Full,
}

#[derive(Debug, Clone, Serialize)]
pub struct FormattedItem {
    pub kind: &'static str,
    pub code: &'static str,
    pub label: String,
    pub value: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct Formatted {
    pub description: String,
    pub items: Vec<FormattedItem>,
}

#[derive(Debug, Clone, Default, Serialize)]
pub struct Remaining {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub text: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
pub struct DecodeResult {
    pub decoded: bool,
    pub decoder: DecoderInfo,
    pub formatted: Formatted,
    pub raw: HashMap<&'static str, RawValue>,
    pub remaining: Remaining,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub message: Option<Message>,
}

impl DecodeResult {
    pub fn new(name: &'static str, description: &str) -> Self {
        Self {
            decoded: false,
            decoder: DecoderInfo {
                name,
                kind: "pattern-match",
                decode_level: DecodeLevel::None,
            },
            formatted: Formatted {
                description: description.to_string(),
                items: Vec::new(),
            },
            raw: HashMap::new(),
            remaining: Remaining::default(),
            message: None,
        }
    }

    pub fn set_message(&mut self, message: &Message) {
        self.message = Some(message.clone());
    }

    pub fn set_decoded(&mut self, decoded: bool) {
        self.decoded = decoded;
        self.decoder.decode_level = if !decoded {
            DecodeLevel::None
        } else if self.remaining.text.is_some() {
            DecodeLevel::Partial
        } else {
            DecodeLevel::Full
        };
    }

    /// Consumes the result and returns a failure variant with `decoded=false`
    /// and the remaining text set. Matches the TS `failUnknown` semantics.
    pub fn fail_unknown(mut self, text: &str) -> Self {
        self.decoded = false;
        self.decoder.decode_level = DecodeLevel::None;
        self.remaining.text = Some(text.to_string());
        self
    }
}

/// The trait every generated plugin implements.
pub trait Plugin: Send + Sync {
    fn name(&self) -> &'static str;
    fn qualifiers(&self) -> Qualifiers;
    fn decode(&self, message: &Message, options: &Options) -> DecodeResult;
}
