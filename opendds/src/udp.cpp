#include "udp.hpp"
#include <stdexcept>
#include <cerrno>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace marine {
struct UdpSource::Impl {
#ifdef _WIN32
  SOCKET socket = INVALID_SOCKET;
  bool started = false;
  ~Impl() { if (socket != INVALID_SOCKET) closesocket(socket); if (started) WSACleanup(); }
#else
  int socket = -1;
  ~Impl() { if (socket >= 0) close(socket); }
#endif
};
UdpSource::UdpSource(const std::string& address) : impl_(new Impl) {
#ifdef _WIN32
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("WSAStartup fallo");
  impl_->started = true;
#endif
  impl_->socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
  if (impl_->socket == INVALID_SOCKET) throw std::runtime_error("no se pudo crear socket UDP");
  BOOL exclusive = TRUE;
  if (setsockopt(impl_->socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
      reinterpret_cast<const char*>(&exclusive), sizeof(exclusive))) throw std::runtime_error("SO_EXCLUSIVEADDRUSE fallo");
#else
  if (impl_->socket < 0) throw std::runtime_error("no se pudo crear socket UDP");
#endif
  const auto colon = address.find(':');
  if (colon == std::string::npos) throw std::runtime_error("endpoint UDP invalido");
  sockaddr_in endpoint{};
  endpoint.sin_family = AF_INET;
  endpoint.sin_port = htons(static_cast<unsigned short>(std::stoul(address.substr(colon + 1))));
  if (inet_pton(AF_INET, address.substr(0, colon).c_str(), &endpoint.sin_addr) != 1)
    throw std::runtime_error("IP UDP invalida");
  if (bind(impl_->socket, reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)))
    throw std::runtime_error("no se pudo abrir NMEA UDP en " + address + "; revisa IP y puerto ocupado");
#ifdef _WIN32
  u_long nonblocking = 1;
  if (ioctlsocket(impl_->socket, FIONBIO, &nonblocking)) throw std::runtime_error("FIONBIO fallo");
#else
  if (fcntl(impl_->socket, F_SETFL, O_NONBLOCK) < 0) throw std::runtime_error("O_NONBLOCK fallo");
#endif
}
UdpSource::~UdpSource() = default;
std::optional<std::string> UdpSource::receive() {
  char buffer[65536];
  const int length = static_cast<int>(recvfrom(impl_->socket, buffer, sizeof(buffer), 0, nullptr, nullptr));
  if (length >= 0) return std::string(buffer, static_cast<std::size_t>(length));
#ifdef _WIN32
  const auto error = WSAGetLastError();
  if (error == WSAEWOULDBLOCK || error == WSAEINTR) return std::nullopt;
#else
  const auto error = errno;
  if (error == EWOULDBLOCK || error == EAGAIN || error == EINTR) return std::nullopt;
#endif
  throw std::runtime_error("error leyendo UDP: " + std::to_string(error));
}
}
