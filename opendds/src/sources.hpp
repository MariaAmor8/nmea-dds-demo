#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace marine {
// Input values; the DDS wire type is generated exclusively from Navigation.idl.
struct Sample {
  std::string raw_nmea;
  std::uint32_t sequence = 0;
  std::uint64_t timestamp_ms = 0;
  double latitude_deg = 0, longitude_deg = 0, speed_knots = 0, course_deg = 0;
  double heading_deg = 0, depth_m = 0;
  bool position_valid = false, heading_valid = false, depth_valid = false, simulated = false;
};
Sample synthetic(std::uint32_t sequence);
Sample parse_gprmc(const std::string& line, std::uint32_t sequence);
std::vector<std::string> datagram_lines(const std::string& payload);
std::string summary(const Sample& sample);
}
