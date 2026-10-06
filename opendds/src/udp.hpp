#pragma once
#include <memory>
#include <optional>
#include <string>

namespace marine {
class UdpSource {
public:
  explicit UdpSource(const std::string& address);
  ~UdpSource();
  UdpSource(const UdpSource&) = delete;
  UdpSource& operator=(const UdpSource&) = delete;
  std::optional<std::string> receive();
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
