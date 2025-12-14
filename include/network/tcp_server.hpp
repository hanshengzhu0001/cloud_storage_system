#ifndef TCP_SERVER_HPP_
#define TCP_SERVER_HPP_

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace banking {
namespace network {

/**
 * TCP Server for banking system operations.
 * Handles client connections and dispatches requests to the banking system.
 */
class TCPServer {
 public:
  using RequestHandler = std::function<std::string(const std::string&)>;

  TCPServer(int port, RequestHandler handler);
  ~TCPServer();

  // Non-copyable
  TCPServer(const TCPServer&) = delete;
  TCPServer& operator=(const TCPServer&) = delete;

  /**
   * Start the server and begin accepting connections.
   */
  bool start();

  /**
   * Stop the server and close all connections.
   */
  void stop();

  /**
   * Check if server is running.
   */
  bool isRunning() const { return running_.load(); }

  /**
   * Get server port.
   */
  int getPort() const { return port_; }

  /**
   * Get number of active connections.
   */
  size_t getConnectionCount() const;

 private:
  void acceptLoop();
  void handleClient(int client_socket, std::string client_addr);

  int port_;
  int server_socket_;
  RequestHandler request_handler_;
  std::atomic<bool> running_;
  std::unique_ptr<std::thread> accept_thread_;
  std::unordered_map<int, std::unique_ptr<std::thread>> client_threads_;
  mutable std::mutex connections_mutex_;
};

}  // namespace network
}  // namespace banking

#endif  // TCP_SERVER_HPP_
