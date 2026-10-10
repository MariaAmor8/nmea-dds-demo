use marine_rustdds::{sequential, Navigation};
use rustdds::{
    no_key::{DeserializerAdapter, SerializerAdapter},
    serialization::{CDRDeserializerAdapter, CDRSerializerAdapter},
};
#[test]
fn idl_generated_navigation_survives_cdr_roundtrip() {
    let mut samples: Vec<_> = [1, 359, 1000].into_iter().map(sequential).collect();
    for flags in 0u8..16 {
        let mut sample = sequential(u32::from(flags) + 1);
        sample.raw_nmea = "GPS · navegación: $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A".to_string();
        sample.position_valid = flags & 1 != 0;
        sample.heading_valid = flags & 2 != 0;
        sample.depth_valid = flags & 4 != 0;
        sample.simulated = flags & 8 != 0;
        samples.push(sample);
    }
    let encoding = CDRSerializerAdapter::<Navigation>::output_encoding();
    assert!(CDRDeserializerAdapter::<Navigation>::supported_encodings().contains(&encoding));
    for a in samples {
        let bytes = CDRSerializerAdapter::<Navigation>::to_bytes(&a).unwrap();
        let b = CDRDeserializerAdapter::<Navigation>::from_bytes(&bytes, encoding).unwrap();
        assert_eq!(a.sequence, b.sequence);
        assert_eq!(a.raw_nmea, b.raw_nmea);
        assert_eq!(a.timestamp_ms, b.timestamp_ms);
        assert_eq!(a.latitude_deg, b.latitude_deg);
        assert_eq!(a.longitude_deg, b.longitude_deg);
        assert_eq!(a.speed_knots, b.speed_knots);
        assert_eq!(a.course_deg, b.course_deg);
        assert_eq!(a.heading_deg, b.heading_deg);
        assert_eq!(a.depth_m, b.depth_m);
        assert_eq!(a.position_valid, b.position_valid);
        assert_eq!(a.heading_valid, b.heading_valid);
        assert_eq!(a.depth_valid, b.depth_valid);
        assert_eq!(a.simulated, b.simulated);
    }
}
