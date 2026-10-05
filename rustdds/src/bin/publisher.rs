use marine_rustdds::nmea::NmeaSource;
use marine_rustdds::*;
use rustdds::{StatusEvented, TopicKind};
use std::{
    sync::atomic::Ordering,
    thread,
    time::{Duration, Instant},
};
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
    let publisher = dp.create_publisher(&q).unwrap();
    let writer = publisher
        .create_datawriter_no_key_cdr::<Navigation>(&topic, None)
        .unwrap();
    let nmea = if c.source == "nmea" {
        Some(NmeaSource::listen(&c.nmea_listen).unwrap())
    } else {
        None
    };
    println!(
        "Publicador listo. Fuente: {}. Ctrl+C para terminar.",
        c.source
    );
    let mut n: u32 = 0;
    let mut next = Instant::now();
    while run.load(Ordering::SeqCst) {
        bridge.check();
        while let Some(event) = writer.try_recv_status() {
            println!("DDS writer: {event:?}");
        }
        if let Some(source) = &nmea {
            if let Some(result) = source.try_next() {
                match result {
                    Ok(sample) => match writer.write(sample.clone(), None) {
                        Ok(()) => println!(
                            "PUBLICADO seq={} speed={:.1} lat={:.5}",
                            sample.sequence, sample.speed_knots, sample.latitude_deg
                        ),
                        Err(e) => eprintln!("Error al publicar: {e:?}"),
                    },
                    Err(error) => eprintln!("{error}"),
                }
            }
        } else if Instant::now() >= next {
            n += 1;
            let sample = sequential(n);
            match writer.write(sample.clone(), None) {
                Ok(()) => println!(
                    "PUBLICADO seq={} speed={:.1} lat={:.5}",
                    n, sample.speed_knots, sample.latitude_deg
                ),
                Err(e) => eprintln!("Error al publicar: {e:?}"),
            }
            next = Instant::now() + Duration::from_secs(1);
        }
        thread::sleep(Duration::from_millis(20));
    }
}
