use marine_rustdds::{sequential, Navigation};
use rustdds::{
    no_key::{DeserializerAdapter, SerializerAdapter},
    serialization::{CDRDeserializerAdapter, CDRSerializerAdapter},
    RepresentationIdentifier,
};
#[test]
fn idl_generated_navigation_survives_cdr_roundtrip() {
    for n in [1, 359, 1000] {
        let a = sequential(n);
        let bytes = CDRSerializerAdapter::<Navigation>::to_bytes(&a).unwrap();
        let b = CDRDeserializerAdapter::<Navigation>::from_bytes(
            &bytes,
            RepresentationIdentifier::CDR_LE,
        )
        .unwrap();
        assert_eq!(a.sequence, b.sequence);
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
