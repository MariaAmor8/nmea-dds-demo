#include "sources.hpp"
#include "config.hpp"
#include "udp.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <functional>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
std::string sentence(const std::string& body) {
  unsigned sum = 0;
  for (unsigned char c : body) sum ^= c;
  std::ostringstream out;
  out << '$' << body << '*' << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << sum;
  return out.str();
}
void rejects(const std::string& line) {
  bool rejected = false;
  try { marine::parse_gprmc(line, 1); } catch (const std::exception&) { rejected = true; }
  require(rejected, "malformed GPRMC accepted");
}
marine::Config config(std::initializer_list<std::string> values, bool publisher = true) {
  std::vector<std::string> strings(values);
  std::vector<char*> args;
  for (auto& v : strings) args.push_back(v.data());
  return marine::Config::parse(static_cast<int>(args.size()), args.data(), publisher);
}
}
int main() {
  try {
    const std::string valid = "GPS1 on UDP2: $GPRMC,131537.83,A,0000.95081,S,00000.29512,W,0010.0,089.0,051026,0.0,W,A,S*66";
    const auto s = marine::parse_gprmc(" \t" + valid + "\r\n", 7);
    require(s.raw_nmea == valid && s.sequence == 7, "raw line/sequence not preserved");
    require(std::abs(s.latitude_deg + 0.0158468333333) < 1e-10, "latitude");
    require(std::abs(s.longitude_deg + 0.0049186666667) < 1e-10, "longitude");
    require(s.speed_knots == 10 && s.course_deg == 89, "speed/course units");
    require(s.timestamp_ms == 1791206137000ULL, "UTC timestamp convention");
    require(s.position_valid && !s.heading_valid && !s.depth_valid && !s.simulated &&
            s.heading_deg == 0 && s.depth_m == 0, "validity flags");
    const auto north = marine::parse_gprmc(sentence("GPRMC,000000,V,9000.0,N,18000.0,E,0,360,290224,0,W"), 2);
    require(!north.position_valid && north.latitude_deg == 90 && north.longitude_deg == 180 && north.course_deg == 360, "V/boundary coordinates");
    rejects(valid.substr(0, valid.size() - 2) + "67");
    rejects("$GPGGA,1*00"); rejects("$GPRMC,1"); rejects(valid + "garbage");
    rejects(sentence("GPRMC,000000,A,0000.0,N,00000.0,E,0,0,310226,0,W"));
    rejects(sentence("GPRMC,000000,A,0000.0,N,00000.0,E,0,0,290225,0,W"));
    for (const auto& body : {
      "GPRMC,240000,A,0000.0,N,00000.0,E,0,0,010126,0,W",
      "GPRMC,000000.,A,0000.0,N,00000.0,E,0,0,010126,0,W",
      "GPRMC,000000,X,0000.0,N,00000.0,E,0,0,010126,0,W",
      "GPRMC,000000,A,0060.0,N,00000.0,E,0,0,010126,0,W",
      "GPRMC,000000,A,9000.1,N,00000.0,E,0,0,010126,0,W",
      "GPRMC,000000,A,0000.0,E,00000.0,E,0,0,010126,0,W",
      "GPRMC,000000,A,0000.0,N,18000.1,E,0,0,010126,0,W",
      "GPRMC,000000,A,,N,00000.0,E,0,0,010126,0,W",
      "GPRMC,000000,A,0000.0,N,00000.0,E,nan,0,010126,0,W",
      "GPRMC,000000,A,0000.0,N,00000.0,E,-1,0,010126,0,W",
      "GPRMC,000000,A,0000.0,N,00000.0,E,0,361,010126,0,W",
      "GPRMC,000000,A,0000.0,N,00000.0,E,0,0,000126,0,W",
      "GPRMC,000000,A,0000.0,N,00000.0,E,0,0,011326,0,W",
      "GPRMC,000000,A,0000.0,N,00000.0,E,0,0,010126"}) rejects(sentence(body));
    const auto lines = marine::datagram_lines(valid + "\r\n\ninvalid\n" + valid + "\n");
    require(lines.size() == 3, "multi-line UDP splitting");
    unsigned sequence = 0, rejected = 0;
    for (const auto& line : lines) {
      try { auto n = marine::parse_gprmc(line, sequence + 1); require(n.sequence == sequence + 1, "sequence"); ++sequence; }
      catch (const std::exception&) { ++rejected; }
    }
    require(sequence == 2 && rejected == 1, "rejected line consumed a sequence");
    const auto generated = marine::synthetic(12);
    require(generated.raw_nmea.empty() && generated.sequence == 12 && generated.simulated &&
            generated.position_valid && generated.heading_valid && generated.depth_valid, "synthetic flags");
    require(std::abs(generated.latitude_deg - 10.00012) < 1e-10 && generated.heading_deg == 14 &&
            std::abs(generated.depth_m - 8.4) < 1e-10 && generated.timestamp_ms > 0, "synthetic values");
    const auto c = config({"test", "--local", "127.0.0.1",
                           "--spdp-port", "17410", "--sedp-port", "17412", "--data-port", "17411"});
    require(c.ini().find("SpdpSendAddrs") == std::string::npos && c.ini().find("SpdpMulticastAddress=239.255.0.1:7400") != std::string::npos &&
            c.ini().find("TTL=1") != std::string::npos && c.ini().find("use_multicast=0") != std::string::npos, "RTPS configuration");
    for (const auto& invalid : {"999.1.1.1", "1.2.3.", "1.2.3.4\ninject", "224.0.0.1"}) {
      bool rejected_config = false;
      try { config({"test", "--local", invalid}); }
      catch (const std::exception&) { rejected_config = true; }
      require(rejected_config, "invalid IP accepted");
    }
    bool duplicate = false;
    try { config({"test", "--local", "127.0.0.1", "--data-port", "7410"}); }
    catch (const std::exception&) { duplicate = true; }
    require(duplicate, "duplicate port accepted");
    for (const auto& flag : {"--peer", "--peer-spdp-port"}) {
      bool rejected = false;
      try { config({"test", "--local", "127.0.0.1", flag, "7410"}); }
      catch (const std::exception&) { rejected = true; }
      require(rejected, "retired argument accepted");
    }
    auto missing = config({"test", "--local", "192.0.2.123"});
    bool missing_rejected = false;
    try { missing.validate_interface(); } catch (const std::exception&) { missing_rejected = true; }
    require(missing_rejected, "nonlocal interface accepted");
    std::cout << "Source/config tests passed\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
