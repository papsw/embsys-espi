use leptos::*;
use serde::Deserialize;
use gloo_timers::future::TimeoutFuture;

#[derive(Clone, Debug, Deserialize)]
struct Tel {
    device_id: String,
    boot_id: String,
    seq: i64,
    uptime_ms: i64,
    rssi: i32,
    free_heap: i32,
    led: i32,
}
#[derive(Clone, Debug, Deserialize)]
struct TelResp { telemetry: Vec<Tel> }

#[derive(Clone, Debug, Deserialize)]
struct Cmd {
    id: i64,
    device_id: String,
    #[serde(rename = "type")]
    kind: String,
    status: String,
}
#[derive(Clone, Debug, Deserialize)]
struct CmdResp { commands: Vec<Cmd> }

async fn get_tel() -> Result<Vec<Tel>, String> {
    let r = reqwasm::http::Request::get("/api/v1/telemetry")
        .send().await.map_err(|e| e.to_string())?;
    r.json::<TelResp>().await.map_err(|e| e.to_string()).map(|d| d.telemetry)
}

async fn get_cmds() -> Result<Vec<Cmd>, String> {
    let r = reqwasm::http::Request::get("/api/v1/commands")
        .send().await.map_err(|e| e.to_string())?;
    r.json::<CmdResp>().await.map_err(|e| e.to_string()).map(|d| d.commands)
}

#[component]
fn App() -> impl IntoView {
    let (tel, set_tel) = create_signal(Vec::<Tel>::new());
    let (cmds, set_cmds) = create_signal(Vec::<Cmd>::new());

    // fetch once then keep refreshing every 3s
    spawn_local(async move {
        loop {
            if let Ok(t) = get_tel().await { set_tel.set(t); }
            if let Ok(c) = get_cmds().await { set_cmds.set(c); }
            TimeoutFuture::new(3_000).await;
        }
    });

    view! {
        <main class="container">
            <h1>"Gateway Dashboard"</h1>
            <p class="sub">"auto-refreshes every 3s"</p>

            <div class="card">
                <h2>"Telemetry"</h2>
                <table role="grid">
                    <thead><tr>
                        <th>"Device"</th><th>"Boot"</th><th>"Seq"</th>
                        <th>"Uptime"</th><th>"RSSI"</th><th>"Heap"</th><th>"LED"</th>
                    </tr></thead>
                    <tbody>
                        <For each=move || tel.get()
                            key=|t| (t.device_id.clone(), t.boot_id.clone(), t.seq)
                            children=move |t| {
                                let rc = if t.rssi > -60 {"good"} else if t.rssi > -75 {"mid"} else {"bad"};
                                let lc = if t.led != 0 {"on"} else {"off"};
                                let lt = if t.led != 0 {"ON"} else {"OFF"};
                                view! {
                                    <tr>
                                        <td>{t.device_id.clone()}</td>
                                        <td>{t.boot_id.clone()}</td>
                                        <td>{t.seq}</td>
                                        <td>{format!("{:.1}s", t.uptime_ms as f64 / 1000.0)}</td>
                                        <td class=rc>{t.rssi}" dBm"</td>
                                        <td>{t.free_heap}</td>
                                        <td class=lc>{lt}</td>
                                    </tr>
                                }
                            }
                        />
                    </tbody>
                </table>
            </div>

            <div class="card">
                <h2>"Commands"</h2>
                <table role="grid">
                    <thead><tr>
                        <th>"ID"</th><th>"Device"</th><th>"Type"</th><th>"Status"</th>
                    </tr></thead>
                    <tbody>
                        <For each=move || cmds.get() key=|c| c.id
                            children=move |c| view! {
                                <tr>
                                    <td>{c.id}</td>
                                    <td>{c.device_id.clone()}</td>
                                    <td>{c.kind.clone()}</td>
                                    <td>{c.status.clone()}</td>
                                </tr>
                            }
                        />
                    </tbody>
                </table>
            </div>
        </main>
    }
}

fn main() {
    mount_to_body(App);
}
