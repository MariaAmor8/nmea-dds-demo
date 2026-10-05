use rustdds::{
    policy::{History, Reliability},
    DomainParticipant, QosPolicies, QosPolicyBuilder,
};
use std::{
    io::{BufRead, BufReader},
    net::Ipv4Addr,
    process::{Child, Command, Stdio},
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
};
include!(concat!(env!("OUT_DIR"), "/navigation.rs"));
pub mod nmea;
pub const TOPIC: &str = "MarineNavigation";

pub struct Config {
    pub local: String,
    pub peer: String,
    pub expected_id: u16,
    pub peer_id: u16,
    pub bridge: bool,
    pub source: String,
    pub nmea_listen: String,
}
impl Config {
    pub fn from_args() -> Self {
        let mut c = Self {
            local: String::new(),
            peer: String::new(),
            expected_id: 0,
            peer_id: 0,
            bridge: true,
            source: "synthetic".to_string(),
            nmea_listen: "127.0.0.1:3100".to_string(),
        };
        let mut args = std::env::args().skip(1);
        while let Some(arg) = args.next() {
            match arg.as_str() {
                "--local" => c.local = args.next().expect("Falta IP local"),
                "--peer" => c.peer = args.next().expect("Falta IP remota"),
                "--expected-id" => c.expected_id = args.next().unwrap().parse().unwrap(),
                "--peer-id" => c.peer_id = args.next().unwrap().parse().unwrap(),
                "--no-bridge" => c.bridge = false,
                "--source" => c.source = args.next().expect("Falta fuente"),
                "--nmea-listen" => c.nmea_listen = args.next().expect("Falta endpoint NMEA"),
                _ => panic!("Opcion desconocida: {arg}"),
            }
            if c.source != "synthetic" && c.source != "nmea" {
                panic!("Fuente invalida: usa --source synthetic o --source nmea");
            }
        }
        if c.bridge {
            c.local
                .parse::<Ipv4Addr>()
                .expect("Usa --local IP_LOCAL --peer IP_REMOTA");
            c.peer
                .parse::<Ipv4Addr>()
                .expect("Usa --local IP_LOCAL --peer IP_REMOTA");
        }
        c
    }
}
pub struct Bridge {
    child: Option<Child>,
}
impl Bridge {
    pub fn start(c: &Config) -> Self {
        if !c.bridge {
            return Self { child: None };
        }
        let script = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("../tools/rtps_unicast_bridge.py");
        let mut child = Command::new("python3")
            .arg(script)
            .args([
                "--local",
                &c.local,
                "--peer",
                &c.peer,
                "--peer-id",
                &c.peer_id.to_string(),
            ])
            .stdout(Stdio::piped())
            .spawn()
            .expect("No se pudo iniciar el auxiliar RTPS");
        let mut ready = String::new();
        let mut reader = BufReader::new(child.stdout.take().unwrap());
        reader
            .read_line(&mut ready)
            .expect("No se pudo leer el estado del auxiliar");
        if ready.trim() != "READY" {
            let _ = child.kill();
            let _ = child.wait();
            panic!("El auxiliar RTPS fallo. Revisa la IP local y los puertos 7400/7401.");
        }
        println!(
            "Auxiliar RTPS preparado: {} -> {} (participant id {})",
            c.local, c.peer, c.peer_id
        );
        Self { child: Some(child) }
    }
    pub fn check(&mut self) {
        if let Some(child) = &mut self.child {
            if let Some(status) = child.try_wait().expect("No se pudo revisar el auxiliar") {
                panic!("El auxiliar RTPS termino: {status}");
            }
        }
    }
}
impl Drop for Bridge {
    fn drop(&mut self) {
        if let Some(child) = &mut self.child {
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}
pub fn running() -> Arc<AtomicBool> {
    let flag = Arc::new(AtomicBool::new(true));
    let f = flag.clone();
    ctrlc::set_handler(move || f.store(false, Ordering::SeqCst)).expect("Ctrl+C handler");
    flag
}
pub fn participant(c: &Config) -> DomainParticipant {
    let p = DomainParticipant::new(0).expect("No se pudo crear el DomainParticipant");
    assert_eq!(
        p.participant_id(),
        c.expected_id,
        "Participant id inesperado. Cierra otros demos DDS de domain 0."
    );
    println!(
        "DDS Domain 0, participant id {}, tipo {}",
        p.participant_id(),
        TYPE_NAME
    );
    p
}
pub fn qos() -> QosPolicies {
    QosPolicyBuilder::new()
        .reliability(Reliability::Reliable {
            max_blocking_time: rustdds::Duration::from_secs(1),
        })
        .history(History::KeepLast { depth: 10 })
        .build()
}
pub fn sequential(n: u32) -> Navigation {
    let timestamp_ms = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64;
    Navigation {
        raw_nmea: String::new(),
        sequence: n,
        timestamp_ms,
        latitude_deg: 10.0 + (n % 1000) as f64 * 0.00001,
        longitude_deg: -75.0 - (n % 1000) as f64 * 0.00001,
        speed_knots: 5.0 + (n % 10) as f64 * 0.1,
        course_deg: (n % 360) as f64,
        heading_deg: ((n + 2) % 360) as f64,
        depth_m: 8.0 + (n % 5) as f64 * 0.2,
        position_valid: true,
        simulated: true,
        heading_valid: true,
        depth_valid: true,
    }
}
