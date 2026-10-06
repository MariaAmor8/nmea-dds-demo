#include "dds.hpp"
#include "udp.hpp"
#include <dds/DCPS/Marked_Default_Qos.h>
#include <iostream>
#include <memory>
#include <thread>

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string(argv[1]) == "--help") { std::cout << marine::usage(true) << '\n'; return 0; }
  try {
    std::cout << std::unitbuf;
    const auto c = marine::Config::parse(argc, argv, true);
    marine::install_signals();
    marine::Session session(c);
    auto publisher = DDS::Publisher_var(session.participant->create_publisher(
      PUBLISHER_QOS_DEFAULT, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK));
    if (!publisher) throw std::runtime_error("create_publisher fallo");
    DDS::DataWriterQos q;
    marine::check(publisher->get_default_datawriter_qos(q), "get_default_datawriter_qos");
    marine::configure_qos(q);
    auto writer = DDS::DataWriter_var(publisher->create_datawriter(session.topic, q, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK));
    if (!writer) throw std::runtime_error("create_datawriter fallo");
    auto typed = Marine::NavigationDataWriter_var(Marine::NavigationDataWriter::_narrow(writer));
    if (!typed) throw std::runtime_error("NavigationDataWriter narrow fallo");
    std::unique_ptr<marine::UdpSource> source;
    if (c.source == "nmea") source.reset(new marine::UdpSource(c.nmea_listen));
    std::cout << "Publicador listo. Fuente: " << c.source;
    if (source) std::cout << " UDP " << c.nmea_listen;
    std::cout << ". Ctrl+C para terminar.\n";
    const auto start = std::chrono::steady_clock::now();
    auto next = start;
    std::uint32_t sequence = 0;
    auto publish = [&](const marine::Sample& s) {
      const auto code = typed->write(marine::to_dds(s), DDS::HANDLE_NIL);
      if (code == DDS::RETCODE_OK) std::cout << "PUBLICADO DDS: " << marine::summary(s) << '\n';
      else std::cerr << "Error al publicar seq=" << s.sequence << ": DDS return code " << code << '\n';
    };
    while (marine::running(c, start)) {
      DDS::PublicationMatchedStatus matched;
      marine::check(writer->get_publication_matched_status(matched), "get_publication_matched_status");
      if (matched.current_count_change || matched.total_count_change)
        std::cout << "PublicationMatched lectores=" << matched.current_count << " total=" << matched.total_count << '\n';
      DDS::OfferedIncompatibleQosStatus incompatible;
      marine::check(writer->get_offered_incompatible_qos_status(incompatible), "get_offered_incompatible_qos_status");
      if (incompatible.total_count_change) std::cerr << "QoS incompatible: policy=" << incompatible.last_policy_id << '\n';
      if (source) {
        // One datagram per iteration keeps shutdown/status polling responsive.
        if (const auto payload = source->receive()) {
          for (const auto& line : marine::datagram_lines(*payload)) {
            try {
              const auto s = marine::parse_gprmc(line, sequence + 1);
              ++sequence;
              std::cout << "RECIBIDO DEL SIMULADOR: " << s.raw_nmea << '\n';
              publish(s);
            } catch (const std::exception& e) { std::cerr << "GPRMC rechazada: " << e.what() << '\n'; }
          }
        }
      } else if (std::chrono::steady_clock::now() >= next) {
        publish(marine::synthetic(++sequence));
        next = std::chrono::steady_clock::now() + std::chrono::seconds(1);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "Publicador terminado.\n";
    return 0;
  } catch (const CORBA::Exception& e) { e._tao_print_exception("OpenDDS publisher"); }
    catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n' << marine::usage(true) << '\n'; }
  return 1;
}
