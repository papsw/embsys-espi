use super::*;

fn test_db() -> Connection {
    init_db(":memory:")
}

#[test]
fn db_tables_exist() {
    let c = test_db();
    let n: i32 = c.query_row("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN ('telemetry','commands')", [], |r| r.get(0)).unwrap();
    assert_eq!(n, 2);
}

#[test]
fn insert_telemetry() {
    let c = test_db();
    c.execute("INSERT INTO telemetry (ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led) VALUES (1000,'esp1','b1',1,500,-60,200000,0)", []).unwrap();
    let cnt: i32 = c.query_row("SELECT COUNT(*) FROM telemetry", [], |r| r.get(0)).unwrap();
    assert_eq!(cnt, 1);
}

#[test]
fn telemetry_dedup() {
    let c = test_db();
    c.execute("INSERT INTO telemetry (ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led) VALUES (1000,'esp1','b1',1,500,-60,200000,0)", []).unwrap();
    c.execute("INSERT OR IGNORE INTO telemetry (ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led) VALUES (2000,'esp1','b1',1,600,-55,199000,1)", []).unwrap();
    let cnt: i32 = c.query_row("SELECT COUNT(*) FROM telemetry", [], |r| r.get(0)).unwrap();
    assert_eq!(cnt, 1);
}

#[test]
fn cmd_lifecycle() {
    let c = test_db();
    c.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (1000,'esp1','set_led','{\"value\":1}','queued')", []).unwrap();
    let id = c.last_insert_rowid();

    let status: String = c.query_row("SELECT status FROM commands WHERE id=?1", [id], |r| r.get(0)).unwrap();
    assert_eq!(status, "queued");

    c.execute("UPDATE commands SET status='sent' WHERE id=?1", [id]).unwrap();
    let status: String = c.query_row("SELECT status FROM commands WHERE id=?1", [id], |r| r.get(0)).unwrap();
    assert_eq!(status, "sent");

    c.execute("UPDATE commands SET status='acked',ack_ts_ms=2000 WHERE id=?1", [id]).unwrap();
    let status: String = c.query_row("SELECT status FROM commands WHERE id=?1", [id], |r| r.get(0)).unwrap();
    assert_eq!(status, "acked");
}

#[test]
fn cmd_fifo() {
    let c = test_db();
    c.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (100,'esp1','cmd1','{}','queued')", []).unwrap();
    c.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (200,'esp1','cmd2','{}','queued')", []).unwrap();
    c.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (300,'esp1','cmd3','{}','queued')", []).unwrap();

    let t: String = c.query_row("SELECT type FROM commands WHERE device_id='esp1' AND status='queued' ORDER BY ts_ms ASC LIMIT 1", [], |r| r.get(0)).unwrap();
    assert_eq!(t, "cmd1");
}

#[test]
fn now_ms_sanity() {
    let t = now_ms();
    assert!(t > 1700000000000);
}

#[test]
fn multi_device_telemetry() {
    let c = test_db();
    c.execute("INSERT INTO telemetry (ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led) VALUES (1000,'esp1','b1',1,500,-60,200000,0)", []).unwrap();
    c.execute("INSERT INTO telemetry (ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led) VALUES (1001,'esp2','b1',1,500,-55,180000,1)", []).unwrap();
    c.execute("INSERT INTO telemetry (ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led) VALUES (1002,'esp1','b1',2,600,-62,199000,0)", []).unwrap();

    let esp1_cnt: i32 = c.query_row("SELECT COUNT(*) FROM telemetry WHERE device_id='esp1'", [], |r| r.get(0)).unwrap();
    let esp2_cnt: i32 = c.query_row("SELECT COUNT(*) FROM telemetry WHERE device_id='esp2'", [], |r| r.get(0)).unwrap();
    assert_eq!(esp1_cnt, 2);
    assert_eq!(esp2_cnt, 1);
}

#[test]
fn cmd_isolation() {
    let c = test_db();
    c.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (100,'esp1','led_on','{}','queued')", []).unwrap();
    c.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (101,'esp2','reboot','{}','queued')", []).unwrap();

    let t: String = c.query_row("SELECT type FROM commands WHERE device_id='esp1' AND status='queued' ORDER BY ts_ms LIMIT 1", [], |r| r.get(0)).unwrap();
    assert_eq!(t, "led_on");

    let t2: String = c.query_row("SELECT type FROM commands WHERE device_id='esp2' AND status='queued' ORDER BY ts_ms LIMIT 1", [], |r| r.get(0)).unwrap();
    assert_eq!(t2, "reboot");
}

#[test]
fn sent_cmd_skipped() {
    let c = test_db();
    c.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (100,'esp1','old','{}','sent')", []).unwrap();
    c.execute("INSERT INTO commands (ts_ms,device_id,type,payload,status) VALUES (200,'esp1','new','{}','queued')", []).unwrap();

    let t: String = c.query_row("SELECT type FROM commands WHERE device_id='esp1' AND status='queued' ORDER BY ts_ms LIMIT 1", [], |r| r.get(0)).unwrap();
    assert_eq!(t, "new");
}

#[test]
fn reboot_new_seq() {
    let c = test_db();
    c.execute("INSERT INTO telemetry (ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led) VALUES (1000,'esp1','boot-a',1,100,-60,200000,0)", []).unwrap();
    c.execute("INSERT INTO telemetry (ts_ms,device_id,boot_id,seq,uptime_ms,rssi,free_heap,led) VALUES (2000,'esp1','boot-b',1,100,-60,200000,0)", []).unwrap();

    let cnt: i32 = c.query_row("SELECT COUNT(*) FROM telemetry WHERE device_id='esp1'", [], |r| r.get(0)).unwrap();
    assert_eq!(cnt, 2); // same seq but diff boot_id = both stored
}

#[test]
fn empty_tables() {
    let c = test_db();
    let tel: i32 = c.query_row("SELECT COUNT(*) FROM telemetry", [], |r| r.get(0)).unwrap();
    let cmd: i32 = c.query_row("SELECT COUNT(*) FROM commands", [], |r| r.get(0)).unwrap();
    assert_eq!(tel, 0);
    assert_eq!(cmd, 0);
}

#[test]
fn indexes_exist() {
    let c = test_db();
    let n: i32 = c.query_row("SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name LIKE 'idx_%'", [], |r| r.get(0)).unwrap();
    assert!(n >= 3);
}
