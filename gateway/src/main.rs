use axum::{
    extract::{Path, Query, State},
    http::StatusCode,
    routing::{get, post},
    Json, Router,
};
use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::sync::{Arc, Mutex};
use tower_http::cors::{Any, CorsLayer};

type Db = Arc<Mutex<Connection>>;


#[derive(Deserialize)]
struct TelemetryIn {
    device_id: String,
    boot_id: String,
    seq: u32,
    uptime_ms: u64,
    rssi: i32,
    free_heap: u32,
    led: u8,
}

#[derive(Deserialize)]
struct NextCmdQuery {
    device_id: String,
}

#[derive(Serialize)]
struct CmdPayload {
    id: i64,
    #[serde(rename = "type")]
    cmd_type: String,
    payload: Value,
}

#[derive(Serialize)]
struct NextCmdResp {
    command: Option<CmdPayload>,
}

#[derive(Deserialize)]
struct QueueCmdIn {
    device_id: String,
    #[serde(rename = "type")]
    cmd_type: String,
    payload: Value,
}


fn init_db(path: &str) -> Connection {
    let conn = Connection::open(path).expect("failed to open db");
    conn.execute_batch("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;").unwrap();
    conn.execute_batch(
        "CREATE TABLE IF NOT EXISTS telemetry (
            id INTEGER PRIMARY KEY,
            ts_ms INTEGER NOT NULL,
            device_id TEXT NOT NULL,
            boot_id TEXT NOT NULL,
            seq INTEGER NOT NULL,
            uptime_ms INTEGER NOT NULL,
            rssi INTEGER NOT NULL,
            free_heap INTEGER NOT NULL,
            led INTEGER NOT NULL,
            UNIQUE(device_id, boot_id, seq)
        );
        CREATE TABLE IF NOT EXISTS commands (
            id INTEGER PRIMARY KEY,
            ts_ms INTEGER NOT NULL,
            device_id TEXT NOT NULL,
            type TEXT NOT NULL,
            payload TEXT NOT NULL,
            status TEXT NOT NULL DEFAULT 'queued',
            ack_ts_ms INTEGER
        );
        CREATE INDEX IF NOT EXISTS idx_tel_ts ON telemetry(ts_ms DESC);
        CREATE INDEX IF NOT EXISTS idx_tel_device ON telemetry(device_id, ts_ms DESC);
        CREATE INDEX IF NOT EXISTS idx_cmd_device ON commands(device_id, status);",
    ).unwrap();
    conn
}

fn now_ms() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as i64
}

async fn post_telemetry(State(db): State<Db>, Json(t): Json<TelemetryIn>) -> (StatusCode, Json<Value>) {
    let conn = db.lock().unwrap();
    let ts = now_ms();
    let r = conn.execute(
        "INSERT OR IGNORE INTO telemetry (ts_ms, device_id, boot_id, seq, uptime_ms, rssi, free_heap, led) VALUES (?1,?2,?3,?4,?5,?6,?7,?8)",
        params![ts, t.device_id, t.boot_id, t.seq, t.uptime_ms, t.rssi, t.free_heap, t.led],
    );
    match r {
        Ok(_) => (StatusCode::OK, Json(json!({"ok": true}))),
        Err(e) => (StatusCode::INTERNAL_SERVER_ERROR, Json(json!({"error": e.to_string()}))),
    }
}

async fn get_next_cmd(State(db): State<Db>, Query(q): Query<NextCmdQuery>) -> (StatusCode, Json<NextCmdResp>) {
    let conn = db.lock().unwrap();
    let mut stmt = conn.prepare("SELECT id, type, payload FROM commands WHERE device_id = ?1 AND status = 'queued' ORDER BY ts_ms ASC LIMIT 1").unwrap();
    let row = stmt.query_row(params![q.device_id], |row| {
        Ok((row.get::<_, i64>(0)?, row.get::<_, String>(1)?, row.get::<_, String>(2)?))
    });
    match row {
        Ok((id, cmd_type, payload_str)) => {
            conn.execute("UPDATE commands SET status='sent' WHERE id=?1", params![id]).ok();
            let payload: Value = serde_json::from_str(&payload_str).unwrap_or(json!({}));
            (StatusCode::OK, Json(NextCmdResp { command: Some(CmdPayload { id, cmd_type, payload }) }))
        }
        Err(_) => (StatusCode::OK, Json(NextCmdResp { command: None })),
    }
}

async fn post_ack(State(db): State<Db>, Path(id): Path<i64>) -> (StatusCode, Json<Value>) {
    let conn = db.lock().unwrap();
    let r = conn.execute("UPDATE commands SET status='acked', ack_ts_ms=?1 WHERE id=?2", params![now_ms(), id]);
    match r {
        Ok(_) => (StatusCode::OK, Json(json!({"ok": true}))),
        Err(e) => (StatusCode::INTERNAL_SERVER_ERROR, Json(json!({"error": e.to_string()}))),
    }
}

async fn get_telemetry(State(db): State<Db>) -> (StatusCode, Json<Value>) {
    let c = db.lock().unwrap();
    let mut s = c.prepare("SELECT ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led FROM telemetry ORDER BY ts_ms DESC LIMIT 50").unwrap();
    let mut out = Vec::new();
    let mut rows = s.query([]).unwrap();
    while let Some(r) = rows.next().unwrap() {
        out.push(json!({"ts_ms": r.get::<_,i64>(0).unwrap(), "device_id": r.get::<_,String>(1).unwrap(),
            "boot_id": r.get::<_,String>(2).unwrap(), "seq": r.get::<_,i64>(3).unwrap(),
            "uptime_ms": r.get::<_,i64>(4).unwrap(), "rssi": r.get::<_,i32>(5).unwrap(),
            "free_heap": r.get::<_,i32>(6).unwrap(), "led": r.get::<_,i32>(7).unwrap()}));
    }
    (StatusCode::OK, Json(json!({"telemetry": out})))
}

async fn post_command(State(db): State<Db>, Json(c): Json<QueueCmdIn>) -> (StatusCode, Json<Value>) {
    let conn = db.lock().unwrap();
    let p = serde_json::to_string(&c.payload).unwrap_or("{}".into());
    match conn.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (?1,?2,?3,?4,'queued')",
        params![now_ms(), c.device_id, c.cmd_type, p]) {
        Ok(_) => (StatusCode::OK, Json(json!({"ok":true,"id":conn.last_insert_rowid()}))),
        Err(e) => (StatusCode::INTERNAL_SERVER_ERROR, Json(json!({"error":e.to_string()}))),
    }
}

async fn get_commands(State(db): State<Db>) -> (StatusCode, Json<Value>) {
    let c = db.lock().unwrap();
    let mut s = c.prepare("SELECT id,ts_ms,device_id,type,payload,status,ack_ts_ms FROM commands ORDER BY ts_ms DESC LIMIT 50").unwrap();
    let mut out = Vec::new();
    let mut rows = s.query([]).unwrap();
    while let Some(r) = rows.next().unwrap() {
        let payload_s: String = r.get(4).unwrap_or_default();
        out.push(json!({"id": r.get::<_,i64>(0).unwrap(), "ts_ms": r.get::<_,i64>(1).unwrap(),
            "device_id": r.get::<_,String>(2).unwrap(), "type": r.get::<_,String>(3).unwrap(),
            "payload": serde_json::from_str::<Value>(&payload_s).unwrap_or(json!({})),
            "status": r.get::<_,String>(5).unwrap(), "ack_ts_ms": r.get::<_,Option<i64>>(6).unwrap()}));
    }
    (StatusCode::OK, Json(json!({"commands": out})))
}

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt::init();

    let conn = init_db("gateway.db");
    let db: Db = Arc::new(Mutex::new(conn));

    let cors = CorsLayer::new().allow_origin(Any).allow_methods(Any).allow_headers(Any);

    let app = Router::new()
        .route("/api/v1/telemetry", post(post_telemetry).get(get_telemetry))
        .route("/api/v1/commands", post(post_command).get(get_commands))
        .route("/api/v1/commands/next", get(get_next_cmd))
        .route("/api/v1/commands/{id}/ack", post(post_ack))
        .layer(cors)
        .with_state(db);

    let addr = "0.0.0.0:8080";
    println!("gateway listening on {}", addr);
    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

#[cfg(test)]
mod tests;