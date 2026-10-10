#include "config.hpp"
#include <sstream>
#include <stdexcept>

namespace marine {
namespace {
unsigned integer(const std::string& s, unsigned max) {
  if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) throw std::runtime_error("entero invalido: " + s);
  const auto value = std::stoul(s);
  if (value > max) throw std::runtime_error("entero fuera de rango: " + s);
  return static_cast<unsigned>(value);
}
void ipv4(const std::string& s) {
  std::istringstream stream(s);
  std::string octet;
  unsigned count = 0;
  while (std::getline(stream, octet, '.')) { integer(octet, 255); ++count; }
  if (count != 4 || s.back() == '.' || s == "0.0.0.0" || integer(s.substr(0, s.find('.')), 255) >= 224)
    throw std::runtime_error("usa IPv4 unicast explicita: " + s);
}
}
std::string usage(bool publisher) {
  return std::string(publisher ? "publisher" : "subscriber") +
    " [--duration SEGUNDOS] [--debug NIVEL]" +
    (publisher ? " [--source synthetic|nmea] [--nmea-listen IP:PUERTO]" : "") +
    " [--spdp-port N --sedp-port N --data-port N]";
}
Config Config::parse(int argc, char* argv[], bool publisher) {
  Config c;
  for (int i = 1; i < argc; ++i) {
    const std::string option = argv[i];
    if (i + 1 >= argc) throw std::runtime_error("falta valor para " + option);
    const std::string value = argv[++i];
    if (option == "--source" && publisher) c.source = value;
    else if (option == "--nmea-listen" && publisher) c.nmea_listen = value;
    else if (option == "--spdp-port") c.spdp_port = integer(value, 65535);
    else if (option == "--sedp-port") c.sedp_port = integer(value, 65535);
    else if (option == "--data-port") c.data_port = integer(value, 65535);
    else if (option == "--duration") c.duration = integer(value, 86400);
    else if (option == "--debug") c.debug = integer(value, 10);
    else throw std::runtime_error("opcion desconocida: " + option);
  }
  if (c.source != "synthetic" && c.source != "nmea") throw std::runtime_error("fuente invalida");
  const auto colon = c.nmea_listen.find(':');
  if (colon == std::string::npos) throw std::runtime_error("--nmea-listen requiere IPv4:puerto");
  ipv4(c.nmea_listen.substr(0, colon));
  if (!integer(c.nmea_listen.substr(colon + 1), 65535)) throw std::runtime_error("puerto NMEA cero");
  if (!c.spdp_port || !c.sedp_port || !c.data_port ||
      c.spdp_port == c.sedp_port || c.spdp_port == c.data_port || c.sedp_port == c.data_port)
    throw std::runtime_error("puertos locales deben ser distintos y no cero");
  return c;
}
std::string Config::ini() const {
  std::ostringstream out;
  out << "[common]\nDCPSDefaultDiscovery=marine_rtps\nDCPSGlobalTransportConfig=marine_transport\n"
      << "DCPSDebugLevel=" << debug
      << "\n[rtps_discovery/marine_rtps]\nSpdpLocalAddress=0.0.0.0:" << spdp_port
      << "\nSedpLocalAddress=0.0.0.0:" << sedp_port
      << "\nSpdpMulticastAddress=239.255.0.1:7400"
      << "\nSedpMulticast=0\nTTL=1\nUndirectedSpdp=1\nPeriodicDirectedSpdp=1\nResendPeriod=1\n"
      << "[config/marine_transport]\ntransports=marine_udp\n"
      << "[transport/marine_udp]\ntransport_type=rtps_udp\nuse_multicast=0\nlocal_address=0.0.0.0:"
      << data_port << '\n';
  return out.str();
}
}
