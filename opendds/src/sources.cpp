#include "sources.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace marine {
namespace {
std::string trim(const std::string& s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
std::vector<std::string> split(const std::string& s, char delimiter) {
  std::vector<std::string> fields;
  std::size_t begin = 0;
  do {
    const auto end = s.find(delimiter, begin);
    fields.push_back(s.substr(begin, end == std::string::npos ? end : end - begin));
    if (end == std::string::npos) break;
    begin = end + 1;
  } while (true);
  return fields;
}
double number(const std::string& text, const char* field) {
  std::size_t used = 0;
  double value;
  try { value = std::stod(text, &used); }
  catch (const std::exception&) { throw std::runtime_error(std::string("numero invalido: ") + field); }
  if (used != text.size() || !std::isfinite(value))
    throw std::runtime_error(std::string("numero invalido: ") + field);
  return value;
}
int digits(const std::string& s) {
  if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
    throw std::runtime_error("fecha/hora o coordenada invalida");
  return std::stoi(s);
}
double coordinate(const std::string& s, const std::string& hemisphere, bool latitude) {
  const std::size_t count = latitude ? 2 : 3;
  if (s.size() <= count) throw std::runtime_error("coordenada incompleta");
  const auto degrees = digits(s.substr(0, count));
  const auto minutes = number(s.substr(count), "minutos");
  if (minutes < 0 || minutes >= 60) throw std::runtime_error("minutos fuera de rango");
  double value = degrees + minutes / 60;
  if (value > (latitude ? 90 : 180)) throw std::runtime_error("coordenada fuera de rango");
  if (hemisphere == (latitude ? "S" : "W")) value = -value;
  else if (hemisphere != (latitude ? "N" : "E")) throw std::runtime_error("hemisferio invalido");
  return value;
}
std::uint64_t timestamp(const std::string& time, const std::string& date) {
  if (date.size() != 6 || time.size() < 6) throw std::runtime_error("fecha/hora incompleta");
  if (time.size() > 6 && (time[6] != '.' || time.size() == 7 ||
      time.substr(7).find_first_not_of("0123456789") != std::string::npos))
    throw std::runtime_error("fraccion horaria invalida");
  const int hour = digits(time.substr(0, 2)), minute = digits(time.substr(2, 2));
  const int second = digits(time.substr(4, 2)), day = digits(date.substr(0, 2));
  const int month = digits(date.substr(2, 2)), year = 2000 + digits(date.substr(4, 2));
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  const int lengths[] = {31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (hour >= 24 || minute >= 60 || second >= 60 || month < 1 || month > 12 ||
      day < 1 || day > lengths[month - 1]) throw std::runtime_error("fecha/hora fuera de rango");
  // Same UTC convention as RustDDS: 2000 + YY and whole seconds.
  const int y = year - (month <= 2);
  const int era = y / 400, yoe = y - era * 400;
  const int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const std::int64_t days = era * 146097LL + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
  return static_cast<std::uint64_t>(days * 86400 + hour * 3600 + minute * 60 + second) * 1000;
}
int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  throw std::runtime_error("checksum no hexadecimal");
}
}
Sample synthetic(std::uint32_t n) {
  Sample s;
  s.sequence = n;
  s.timestamp_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count());
  s.latitude_deg = 10.0 + (n % 1000) * 0.00001;
  s.longitude_deg = -75.0 - (n % 1000) * 0.00001;
  s.speed_knots = 5.0 + (n % 10) * 0.1;
  s.course_deg = n % 360;
  s.heading_deg = ((static_cast<std::uint64_t>(n) + 2) % 360);
  s.depth_m = 8.0 + (n % 5) * 0.2;
  s.position_valid = s.heading_valid = s.depth_valid = s.simulated = true;
  return s;
}
Sample parse_gprmc(const std::string& line, std::uint32_t sequence) {
  Sample s;
  s.raw_nmea = trim(line);
  if (s.raw_nmea.find('\0') != std::string::npos) throw std::runtime_error("NUL en sentencia");
  const auto start = s.raw_nmea.find("$GPRMC");
  if (start == std::string::npos) throw std::runtime_error("no es GPRMC");
  const auto sentence = s.raw_nmea.substr(start);
  const auto star = sentence.find('*');
  if (star == std::string::npos || star + 3 != sentence.size()) throw std::runtime_error("checksum incompleto o sufijo inesperado");
  const auto body = sentence.substr(1, star - 1);
  unsigned checksum = 0;
  for (unsigned char c : body) checksum ^= c;
  if (checksum != static_cast<unsigned>(hex(sentence[star + 1]) * 16 + hex(sentence[star + 2])))
    throw std::runtime_error("checksum incorrecto");
  const auto fields = split(body, ',');
  if (fields.size() < 12 || fields[0] != "GPRMC") throw std::runtime_error("campos GPRMC incompletos");
  if (fields[2] != "A" && fields[2] != "V") throw std::runtime_error("estado GPS invalido");
  s.position_valid = fields[2] == "A";
  s.latitude_deg = coordinate(fields[3], fields[4], true);
  s.longitude_deg = coordinate(fields[5], fields[6], false);
  s.speed_knots = number(fields[7], "velocidad");
  s.course_deg = number(fields[8], "curso");
  if (s.speed_knots < 0 || s.course_deg < 0 || s.course_deg > 360) throw std::runtime_error("velocidad/curso fuera de rango");
  s.timestamp_ms = timestamp(fields[1], fields[9]);
  s.sequence = sequence;
  return s;
}
std::vector<std::string> datagram_lines(const std::string& payload) {
  auto lines = split(payload, '\n');
  std::vector<std::string> result;
  for (auto& line : lines) { line = trim(line); if (!line.empty()) result.push_back(line); }
  return result;
}
std::string summary(const Sample& s) {
  std::ostringstream out;
  out << std::boolalpha << std::fixed << std::setprecision(5)
      << "seq=" << s.sequence << " timestamp_ms=" << s.timestamp_ms
      << " lat=" << s.latitude_deg << " lon=" << s.longitude_deg << std::setprecision(1)
      << " speed=" << s.speed_knots << " kn course=" << s.course_deg << " heading=";
  if (s.heading_valid) out << s.heading_deg; else out << "N/D";
  out << " depth=";
  if (s.depth_valid) out << s.depth_m << " m"; else out << "N/D";
  out << " position_valid=" << s.position_valid << " heading_valid=" << s.heading_valid
      << " depth_valid=" << s.depth_valid << " simulated=" << s.simulated;
  return out.str();
}
}
