#include "dds.hpp"
#include <dds/DCPS/Marked_Default_Qos.h>
#include <iostream>
#include <optional>
#include <thread>

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string(argv[1]) == "--help") { std::cout << marine::usage(false) << '\n'; return 0; }
  try {
    std::cout << std::unitbuf;
    const auto c = marine::Config::parse(argc, argv, false);
    marine::install_signals();
    marine::Session session(c);
    auto subscriber = DDS::Subscriber_var(session.participant->create_subscriber(
      SUBSCRIBER_QOS_DEFAULT, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK));
    if (!subscriber) throw std::runtime_error("create_subscriber fallo");
    DDS::DataReaderQos q;
    marine::check(subscriber->get_default_datareader_qos(q), "get_default_datareader_qos");
    marine::configure_qos(q);
    auto reader = DDS::DataReader_var(subscriber->create_datareader(session.topic, q, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK));
    if (!reader) throw std::runtime_error("create_datareader fallo");
    auto typed = Marine::NavigationDataReader_var(Marine::NavigationDataReader::_narrow(reader));
    if (!typed) throw std::runtime_error("NavigationDataReader narrow fallo");
    std::cout << "Suscriptor listo. Esperando MarineNavigation. Ctrl+C para terminar.\n";
    std::optional<std::uint32_t> previous;
    const auto start = std::chrono::steady_clock::now();
    while (marine::running(c, start)) {
      DDS::SubscriptionMatchedStatus matched;
      marine::check(reader->get_subscription_matched_status(matched), "get_subscription_matched_status");
      if (matched.current_count_change || matched.total_count_change)
        std::cout << "SubscriptionMatched escritores=" << matched.current_count << " total=" << matched.total_count << '\n';
      DDS::RequestedIncompatibleQosStatus incompatible;
      marine::check(reader->get_requested_incompatible_qos_status(incompatible), "get_requested_incompatible_qos_status");
      if (incompatible.total_count_change) std::cerr << "QoS incompatible: policy=" << incompatible.last_policy_id << '\n';
      // Bound each drain so a continuous stream cannot prevent shutdown.
      for (unsigned i = 0; i < 100 && marine::running(c, start); ++i) {
        Marine::Navigation sample;
        DDS::SampleInfo info;
        const auto code = typed->take_next_sample(sample, info);
        if (code == DDS::RETCODE_NO_DATA) break;
        marine::check(code, "take_next_sample");
        if (!info.valid_data) continue;
        const auto s = marine::from_dds(sample);
        if (previous && s.sequence != static_cast<std::uint32_t>(*previous + 1))
          std::cout << "SALTO secuencia " << *previous << " -> " << s.sequence << '\n';
        previous = s.sequence;
        std::cout << "RECIBIDO DDS: " << s.raw_nmea << "\nPARSEADO: " << marine::summary(s) << '\n';
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "Suscriptor terminado.\n";
    return 0;
  } catch (const CORBA::Exception& e) { e._tao_print_exception("OpenDDS subscriber"); }
    catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n' << marine::usage(false) << '\n'; }
  return 1;
}
