use axum::{
    extract::ws::{Message, WebSocket, WebSocketUpgrade},
    routing::get,
    response::IntoResponse,
    Router,
};

async fn ws_handler(ws: WebSocketUpgrade) -> impl IntoResponse {
    ws.on_upgrade(handle_ws)
}

async fn handle_ws(mut socket: WebSocket) {
    println!("worker connected");
    while let Some(Ok(msg)) = socket.recv().await {
        if let Message::Text(text) = msg {
            println!("ws msg: {}", text);
        }
    }
    println!("worker disconnected");
}

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt::init();
    let app = Router::new()
        .route("/", get(|| async {"coordinator"}))
        .route("/ws", get(ws_handler));
    let listener = tokio::net::TcpListener::bind("0.0.0.0:8080").await.unwrap();
    println!("Listening on {}", listener.local_addr().unwrap());
    axum::serve(listener, app).await.unwrap();
}
