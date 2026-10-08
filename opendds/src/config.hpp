#pragma once
#include <string>

namespace marine {
struct Config {
  std::string local, interface, source = "synthetic", nmea_listen = "127.0.0.1:3100";
  unsigned spdp_port = 7410, sedp_port = 7412, data_port = 7411;
  unsigned duration = 0, debug = 0;
  static Config parse(int argc, char* argv[], bool publisher);
  std::string ini() const;
  void validate_interface();
};
std::string usage(bool publisher);
}
