use super::Navigation;
use std::{
    net::UdpSocket,
    sync::mpsc::{self, Receiver},
    thread,
    time::Duration,
};

#[derive(Debug)]
pub enum ParseError {
    NotGprmc,
    MissingChecksum,
    InvalidChecksum,
    InvalidField(&'static str),
    InvalidNumber(&'static str),
    InvalidCoordinate(&'static str),
    InvalidDateTime,
}

pub fn parse_gprmc(line: &str, sequence: u32) -> Result<Navigation, ParseError> {
    let raw_nmea = line.trim().to_string();
    let start = line.find("$GPRMC").ok_or(ParseError::NotGprmc)?;
    let sentence = line[start..].trim();
    let star = sentence.rfind('*').ok_or(ParseError::MissingChecksum)?;
    let body = sentence.get(1..star).ok_or(ParseError::MissingChecksum)?;
    let received = sentence
        .get(star + 1..)
        .ok_or(ParseError::MissingChecksum)?;
    if received.len() < 2 {
        return Err(ParseError::MissingChecksum);
    }
    let expected =
        u8::from_str_radix(&received[..2], 16).map_err(|_| ParseError::InvalidChecksum)?;
    let actual = body.bytes().fold(0u8, |checksum, byte| checksum ^ byte);
    if actual != expected {
        return Err(ParseError::InvalidChecksum);
    }

    let fields: Vec<&str> = body.split(',').collect();
    if fields.first() != Some(&"GPRMC") || fields.len() < 12 {
        return Err(ParseError::InvalidField("GPRMC fields"));
    }
    let position_valid = match fields[2] {
        "A" => true,
        "V" => false,
        _ => return Err(ParseError::InvalidField("status")),
    };
    let latitude_deg = coordinate(fields[3], fields[4], 2, "latitude")?;
    let longitude_deg = coordinate(fields[5], fields[6], 3, "longitude")?;
    let speed_knots = number(fields[7], "speed")?;
    let course_deg = number(fields[8], "course")?;
    if !(0.0..=360.0).contains(&course_deg) {
        return Err(ParseError::InvalidNumber("course"));
    }
    let timestamp_ms = nmea_timestamp(fields[1], fields[9])?;

    Ok(Navigation {
        raw_nmea,
        sequence,
        timestamp_ms,
        latitude_deg,
        longitude_deg,
        speed_knots,
        course_deg,
        heading_deg: 0.0,
        depth_m: 0.0,
        position_valid,
        heading_valid: false,
        depth_valid: false,
        simulated: false,
    })
}

fn number(value: &str, field: &'static str) -> Result<f64, ParseError> {
    value.parse().map_err(|_| ParseError::InvalidNumber(field))
}

fn coordinate(
    value: &str,
    hemisphere: &str,
    degrees_digits: usize,
    field: &'static str,
) -> Result<f64, ParseError> {
    if value.len() <= degrees_digits {
        return Err(ParseError::InvalidCoordinate(field));
    }
    let degrees: f64 = value[..degrees_digits]
        .parse()
        .map_err(|_| ParseError::InvalidCoordinate(field))?;
    let minutes: f64 = value[degrees_digits..]
        .parse()
        .map_err(|_| ParseError::InvalidCoordinate(field))?;
    if minutes >= 60.0 {
        return Err(ParseError::InvalidCoordinate(field));
    }
    let mut result = degrees + minutes / 60.0;
    let valid = match (field, hemisphere) {
        ("latitude", "N") | ("longitude", "E") => true,
        ("latitude", "S") | ("longitude", "W") => {
            result = -result;
            true
        }
        _ => false,
    };
    if !valid
        || (field == "latitude" && result.abs() > 90.0)
        || (field == "longitude" && result.abs() > 180.0)
    {
        return Err(ParseError::InvalidCoordinate(field));
    }
    Ok(result)
}

fn nmea_timestamp(time: &str, date: &str) -> Result<u64, ParseError> {
    if time.len() < 6 || date.len() != 6 {
        return Err(ParseError::InvalidDateTime);
    }
    let hour: u64 = time[0..2]
        .parse()
        .map_err(|_| ParseError::InvalidDateTime)?;
    let minute: u64 = time[2..4]
        .parse()
        .map_err(|_| ParseError::InvalidDateTime)?;
    let second: u64 = time[4..6]
        .parse()
        .map_err(|_| ParseError::InvalidDateTime)?;
    let day: i64 = date[0..2]
        .parse()
        .map_err(|_| ParseError::InvalidDateTime)?;
    let month: i64 = date[2..4]
        .parse()
        .map_err(|_| ParseError::InvalidDateTime)?;
    let year: i64 = 2000
        + date[4..6]
            .parse::<i64>()
            .map_err(|_| ParseError::InvalidDateTime)?;
    if hour >= 24
        || minute >= 60
        || second >= 60
        || !(1..=31).contains(&day)
        || !(1..=12).contains(&month)
    {
        return Err(ParseError::InvalidDateTime);
    }
    let days = days_from_civil(year, month, day);
    let seconds = days * 86_400 + (hour * 3600 + minute * 60 + second) as i64;
    u64::try_from(seconds)
        .map(|s| s * 1000)
        .map_err(|_| ParseError::InvalidDateTime)
}

fn days_from_civil(year: i64, month: i64, day: i64) -> i64 {
    let y = year - i64::from(month <= 2);
    let era = if y >= 0 { y } else { y - 399 } / 400;
    let yoe = y - era * 400;
    let month_adjusted = month + if month > 2 { -3 } else { 9 };
    let doy = (153 * month_adjusted + 2) / 5 + day - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146097 + doe - 719468
}

pub struct NmeaSource {
    receiver: Receiver<Result<Navigation, String>>,
}

impl NmeaSource {
    pub fn listen(address: &str) -> Result<Self, String> {
        let socket = UdpSocket::bind(address)
            .map_err(|e| format!("No se pudo abrir NMEA UDP en {address}: {e}"))?;
        socket
            .set_nonblocking(true)
            .map_err(|e| format!("No se pudo configurar NMEA UDP: {e}"))?;
        let (sender, receiver) = mpsc::channel();
        thread::spawn(move || {
            let mut sequence = 0;
            let mut buffer = [0u8; 4096];
            loop {
                match socket.recv_from(&mut buffer) {
                    Ok((length, peer)) => {
                        let payload = String::from_utf8_lossy(&buffer[..length]);
                        println!("Datagrama NMEA recibido desde {peer}");
                        for line in payload.lines() {
                            let parsed = parse_gprmc(line, sequence + 1)
                                .map_err(|e| format!("GPRMC rechazada: {e:?}"));
                            if parsed.is_ok() {
                                sequence += 1;
                            }
                            if sender.send(parsed).is_err() {
                                return;
                            }
                        }
                    }
                    Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                        thread::sleep(Duration::from_millis(10));
                    }
                    Err(e) => {
                        let _ = sender.send(Err(format!("Error leyendo NMEA UDP: {e}")));
                        return;
                    }
                }
            }
        });
        Ok(Self { receiver })
    }

    pub fn try_next(&self) -> Option<Result<Navigation, String>> {
        self.receiver.try_recv().ok()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const VALID: &str = "GPS1 on UDP2: $GPRMC,131537.83,A,0000.95081,S,00000.29512,W,0010.0,089.0,051026,0.0,W,A,S*66";

    #[test]
    fn parses_prefixed_gprmc() {
        let n = parse_gprmc(VALID, 7).unwrap();
        assert_eq!(n.raw_nmea, VALID);
        assert_eq!(n.sequence, 7);
        assert!((n.latitude_deg + 0.0158468333).abs() < 1e-9);
        assert!((n.longitude_deg + 0.0049186667).abs() < 1e-9);
        assert_eq!(n.speed_knots, 10.0);
        assert_eq!(n.course_deg, 89.0);
        assert!(!n.heading_valid && !n.depth_valid && !n.simulated);
    }

    #[test]
    fn rejects_bad_checksum() {
        assert!(matches!(
            parse_gprmc(&VALID.replace("*66", "*67"), 1),
            Err(ParseError::InvalidChecksum)
        ));
    }
}
