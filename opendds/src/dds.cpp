#include "dds.hpp"
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/StaticIncludes.h>
#if OPENDDS_DO_MANUAL_STATIC_INCLUDES
#include <dds/DCPS/RTPS/RtpsDiscovery.h>
#include <dds/DCPS/transport/rtps_udp/RtpsUdp.h>
#endif
#include <ace/OS_NS_unistd.h>
#include <ace/SString.h>
#include <csignal>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace marine {
namespace {
std::atomic<bool> stop{false};
static_assert(std::atomic<bool>::is_always_lock_free, "signal handler needs lock-free atomics");
void signal_handler(int) { stop.store(true, std::memory_order_relaxed); }
#ifdef _WIN32
BOOL WINAPI console_handler(DWORD event) {
  if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) { stop.store(true, std::memory_order_relaxed); return TRUE; }
  return FALSE;
}
#endif
template<class Qos> void qos_common(Qos& q) {
  q.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
  q.reliability.max_blocking_time.sec = 1;
  q.reliability.max_blocking_time.nanosec = 0;
  q.history.kind = DDS::KEEP_LAST_HISTORY_QOS;
  q.history.depth = 10;
  q.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
}
}
void configure_qos(DDS::DataWriterQos& q) { qos_common(q); }
void configure_qos(DDS::DataReaderQos& q) { qos_common(q); }
void check(DDS::ReturnCode_t code, const char* action) {
  if (code != DDS::RETCODE_OK) throw std::runtime_error(std::string(action) + ": DDS return code " + std::to_string(code));
}
void install_signals() {
  stop.store(false, std::memory_order_relaxed);
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
#ifdef _WIN32
  if (!SetConsoleCtrlHandler(console_handler, TRUE)) throw std::runtime_error("SetConsoleCtrlHandler fallo");
#endif
}
bool running(const Config& c, std::chrono::steady_clock::time_point start) {
  return !stop.load(std::memory_order_relaxed) && (!c.duration || std::chrono::steady_clock::now() - start < std::chrono::seconds(c.duration));
}
Marine::Navigation to_dds(const Sample& s) {
  Marine::Navigation n;
  n.raw_nmea = s.raw_nmea.c_str();
  n.sequence = s.sequence; n.timestamp_ms = s.timestamp_ms;
  n.latitude_deg = s.latitude_deg; n.longitude_deg = s.longitude_deg;
  n.speed_knots = s.speed_knots; n.course_deg = s.course_deg;
  n.heading_deg = s.heading_deg; n.depth_m = s.depth_m;
  n.position_valid = s.position_valid; n.heading_valid = s.heading_valid;
  n.depth_valid = s.depth_valid; n.simulated = s.simulated;
  return n;
}
Sample from_dds(const Marine::Navigation& n) {
  Sample s;
  s.raw_nmea = n.raw_nmea.in();
  s.sequence = n.sequence; s.timestamp_ms = n.timestamp_ms;
  s.latitude_deg = n.latitude_deg; s.longitude_deg = n.longitude_deg;
  s.speed_knots = n.speed_knots; s.course_deg = n.course_deg;
  s.heading_deg = n.heading_deg; s.depth_m = n.depth_m;
  s.position_valid = n.position_valid; s.heading_valid = n.heading_valid;
  s.depth_valid = n.depth_valid; s.simulated = n.simulated;
  return s;
}
Session::Session(const Config& c) {
  try {
    config_path_ = std::filesystem::temp_directory_path() / ("marine-opendds-" +
      std::to_string(ACE_OS::getpid()) + "-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".ini");
    {
      std::ofstream file(config_path_);
      if (!file || !(file << c.ini())) throw std::runtime_error("no se pudo crear configuracion RTPS temporal");
    }
    ACE_TString name(ACE_TEXT("marine")), option(ACE_TEXT("-DCPSConfigFile"));
    const std::string path = config_path_.string();
    ACE_TString file(ACE_TEXT_CHAR_TO_TCHAR(path.c_str()));
    int argc = 3;
    ACE_TCHAR* argv[] = {const_cast<ACE_TCHAR*>(name.c_str()), const_cast<ACE_TCHAR*>(option.c_str()),
                        const_cast<ACE_TCHAR*>(file.c_str()), nullptr};
    factory_ = TheParticipantFactoryWithArgs(argc, argv);
    if (!factory_) throw std::runtime_error("no se pudo inicializar OpenDDS");
    participant = factory_->create_participant(0, PARTICIPANT_QOS_DEFAULT, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (!participant) throw std::runtime_error("create_participant fallo; revisa IP local y puertos RTPS");
    Marine::NavigationTypeSupport_var type = new Marine::NavigationTypeSupportImpl;
    check(type->register_type(participant, "Marine::Navigation"), "register_type");
    topic = participant->create_topic("MarineNavigation", "Marine::Navigation", TOPIC_QOS_DEFAULT,
                                     nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (!topic) throw std::runtime_error("create_topic fallo");
    std::cout << "DDS Domain 0, Topic MarineNavigation, tipo Marine::Navigation, NoKey\n"
              << "RTPS unicast: " << c.local << " SPDP=" << c.spdp_port << " SEDP=" << c.sedp_port
              << " datos=" << c.data_port << " -> " << c.peer << ':' << c.peer_spdp_port << '\n';
  } catch (...) { cleanup(); throw; }
}
void Session::cleanup() noexcept {
  try {
    topic = DDS::Topic::_nil();
    if (participant) {
      const auto contained = participant->delete_contained_entities();
      const auto deleted = factory_->delete_participant(participant);
      if (contained != DDS::RETCODE_OK || deleted != DDS::RETCODE_OK)
        std::cerr << "Error liberando entidades DDS: " << contained << ", " << deleted << '\n';
      participant = DDS::DomainParticipant::_nil();
    }
    if (factory_) { TheServiceParticipant->shutdown(); factory_ = DDS::DomainParticipantFactory::_nil(); }
  } catch (...) { std::cerr << "Error durante cierre OpenDDS\n"; }
  std::error_code error;
  if (!config_path_.empty()) std::filesystem::remove(config_path_, error);
}
Session::~Session() { cleanup(); }
}
