//! Cross-plugin data types (Route, Waypoint, Wind). Mirrors `runtimes/typescript/types/`.

use serde::Serialize;

#[derive(Debug, Clone, Serialize)]
pub struct Waypoint {
    pub name: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub time: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub latitude: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub longitude: Option<f64>,
}

#[derive(Debug, Clone, Serialize)]
pub struct Route {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
    pub waypoints: Vec<Waypoint>,
}

#[derive(Debug, Clone, Serialize)]
pub struct Wind {
    pub direction: f64,
    pub speed: f64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub altitude: Option<f64>,
}
