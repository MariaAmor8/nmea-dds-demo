use marine_rustdds::*;
use rustdds::TopicKind;
use std::{sync::atomic::Ordering, thread, time::Duration};
fn main() {
    env_logger::init();
    let c = Config::from_args();
    let run = running();
    let mut bridge = Bridge::start(&c);
    let dp = participant(&c);
    let q = qos();
    let topic = dp
        .create_topic(
            TOPIC.to_string(),
            TYPE_NAME.to_string(),
            &q,
            TopicKind::NoKey,
        )
        .unwrap();
    let subscriber = dp.create_subscriber(&q).unwrap();
    let mut reader = subscriber
        .create_datareader_no_key_cdr::<Navigation>(&topic, None)
        .unwrap();
    println!("Suscriptor listo. Esperando MarineNavigation. Ctrl+C para terminar.");
    let mut previous: Option<u32> = None;
    while run.load(Ordering::SeqCst) {
        bridge.check();
        loop {
            match reader.take_next_sample() {
                Ok(Some(sample)) => {
                    let v = sample.into_value();
                    if let Some(p) = previous {
                        if v.sequence != p + 1 {
                            println!("SALTO secuencia {} -> {}", p, v.sequence);
                        }
                    }
                    previous = Some(v.sequence);
                    println!("RECIBIDO DDS: {}\nPARSEADO: seq={} lat={:.5} lon={:.5} speed={:.1} kn course={:.1} heading={} depth={} position_valid={} simulated={}",
                        v.raw_nmea, v.sequence, v.latitude_deg, v.longitude_deg, v.speed_knots, v.course_deg,
                        if v.heading_valid { format!("{:.1}", v.heading_deg) } else { "N/D".to_string() },
                        if v.depth_valid { format!("{:.1} m", v.depth_m) } else { "N/D".to_string() },
                        v.position_valid, v.simulated);
                }
                Ok(None) => break,
                Err(e) => {
                    eprintln!("Error DDS al recibir: {e:?}");
                    break;
                }
            }
        }
        thread::sleep(Duration::from_millis(20));
    }
}
