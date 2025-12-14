#ifndef TCP_CLIENT_HPP_
#define TCP_CLIENT_HPP_

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace banking {
namespace network {

/**
 * TCP Client for connecting to banking system servers.
 * Handles connection management and request/response communication.
 */
class TCPClient {
 public:
  TCPClient(const std::string& host, int port);
  ~TCPClient();

  // Non-copyable
  TCPClient(const TCPClient&) = delete;
  TCPClient& operator=(const TCPClient&) = delete;

  /**
   * Connect to the server.
   */
  bool connect();

  /**
   * Disconnect from the server.
   */
  void disconnect();

  /**
   * Check if client is connected.
   */
  bool isConnected() const { return connected_.load(); }

  /**
   * Send a request and receive response synchronously.
   */
  std::string sendRequest(const std::string& request);

  /**
   * Send a request asynchronously (fire and forget).
   */
  bool sendRequestAsync(const std::string& request);

  /**
   * Get server host.
   */
  const std::string& getHost() const { return host_; }

  /**
   * Get server port.
   */
  int getPort() const { return port_; }

 private:
  void receiveLoop();
  bool sendMessage(const std::string& message);
  std::string receiveMessage();

  std::string host_;
  int port_;
  int socket_;
  std::atomic<bool> connected_;
  std::unique_ptr<std::thread> receive_thread_;
  mutable std::mutex socket_mutex_;
  std::mutex response_mutex_;
  std::condition_variable response_cv_;
  std::string pending_response_;
  bool response_ready_;
};

}  // namespace network
}  // namespace banking

#endif  // TCP_CLIENT_HPP_
