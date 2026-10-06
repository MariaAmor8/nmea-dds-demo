#pragma once
#include "config.hpp"
#include "sources.hpp"
#include "NavigationTypeSupportImpl.h"
#include <dds/DCPS/Service_Participant.h>
#include <chrono>
#include <filesystem>

namespace marine {
Marine::Navigation to_dds(const Sample& sample);
Sample from_dds(const Marine::Navigation& sample);
void check(DDS::ReturnCode_t code, const char* action);
void install_signals();
bool running(const Config& config, std::chrono::steady_clock::time_point start);
class Session {
public:
  explicit Session(const Config& config);
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  DDS::DomainParticipant_var participant;
  DDS::Topic_var topic;
private:
  DDS::DomainParticipantFactory_var factory_;
  std::filesystem::path config_path_;
  void cleanup() noexcept;
};
void configure_qos(DDS::DataWriterQos& qos);
void configure_qos(DDS::DataReaderQos& qos);
}
