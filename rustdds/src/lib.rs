use rustdds::{
    policy::{History, Reliability},
    DomainParticipant, QosPolicies, QosPolicyBuilder,
};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
include!(concat!(env!("OUT_DIR"), "/navigation.rs"));
pub mod nmea;
pub const TOPIC: &str = "MarineNavigation";

pub struct Config {
    pub expected_id: u16,
    pub source: String,
    pub nmea_listen: String,
}
impl Config {
    pub fn from_args() -> Self {
        let mut c = Self {
            expected_id: 0,
            source: "synthetic".to_string(),
            nmea_listen: "127.0.0.1:3100".to_string(),
        };
        let mut args = std::env::args().skip(1);
        while let Some(arg) = args.next() {
            match arg.as_str() {
                "--help" | "-h" => {
                    println!("Uso: publisher|subscriber [--expected-id N] [--source synthetic|nmea] [--nmea-listen IP:PUERTO]");
                    std::process::exit(0);
                }
                "--expected-id" => c.expected_id = args.next().unwrap().parse().unwrap(),
                "--source" => c.source = args.next().expect("Falta fuente"),
                "--nmea-listen" => c.nmea_listen = args.next().expect("Falta endpoint NMEA"),
                _ => panic!("Opcion desconocida: {arg}"),
            }
            if c.source != "synthetic" && c.source != "nmea" {
                panic!("Fuente invalida: usa --source synthetic o --source nmea");
            }
        }
        c
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
    println!(
        "SPDP multicast nativo RustDDS: interfaces y sockets predeterminados de la biblioteca"
    );
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
