#include "tcp_server.hpp"
#include "protocol.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <vector>

namespace banking {
namespace network {

TCPServer::TCPServer(int port, RequestHandler handler)
    : port_(port),
      server_socket_(-1),
      request_handler_(std::move(handler)),
      running_(false) {
}

TCPServer::~TCPServer() {
  stop();
}

bool TCPServer::start() {
  // Create socket
  server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket_ < 0) {
    std::cerr << "Failed to create socket" << std::endl;
    return false;
  }

  // Set socket options
  int opt = 1;
  if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                 &opt, sizeof(opt)) < 0) {
    std::cerr << "Failed to set socket options" << std::endl;
    close(server_socket_);
    return false;
  }

  // Bind socket
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);

  if (bind(server_socket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
    std::cerr << "Failed to bind socket to port " << port_ << std::endl;
    close(server_socket_);
    return false;
  }

  // Listen for connections
  if (listen(server_socket_, 10) < 0) {
    std::cerr << "Failed to listen on socket" << std::endl;
    close(server_socket_);
    return false;
  }

  running_ = true;
  accept_thread_ = std::make_unique<std::thread>(&TCPServer::acceptLoop, this);

  std::cout << "TCP Server started on port " << port_ << std::endl;
  return true;
}

void TCPServer::stop() {
  if (!running_) return;

  running_ = false;

  // Close server socket to break accept loop
  if (server_socket_ >= 0) {
    shutdown(server_socket_, SHUT_RDWR);
    close(server_socket_);
    server_socket_ = -1;
  }

  // Wait for accept thread
  if (accept_thread_ && accept_thread_->joinable()) {
    accept_thread_->join();
  }

  // Close all client connections
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& pair : client_threads_) {
      if (pair.second && pair.second->joinable()) {
        pair.second->join();
      }
    }
    client_threads_.clear();
  }

  std::cout << "TCP Server stopped" << std::endl;
}

void TCPServer::acceptLoop() {
  while (running_) {
    struct sockaddr_in client_address;
    socklen_t client_addr_len = sizeof(client_address);

    int client_socket = accept(server_socket_,
                              (struct sockaddr*)&client_address,
                              &client_addr_len);

    if (client_socket < 0) {
      if (running_) {
        std::cerr << "Failed to accept connection" << std::endl;
      }
      continue;
    }

    // Get client address for logging
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::string client_addr = std::string(client_ip) + ":" +
                             std::to_string(ntohs(client_address.sin_port));

    std::cout << "Accepted connection from " << client_addr << std::endl;

    // Handle client in separate thread
    {
      std::lock_guard<std::mutex> lock(connections_mutex_);
      client_threads_[client_socket] =
          std::make_unique<std::thread>(&TCPServer::handleClient, this,
                                       client_socket, client_addr);
    }
  }
}

void TCPServer::handleClient(int client_socket, std::string client_addr) {
  char buffer[4096];
  std::string message_buffer;

  while (running_) {
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));

    if (bytes_read <= 0) {
      if (bytes_read < 0) {
        std::cerr << "Error reading from client " << client_addr << std::endl;
      }
      break;
    }

    message_buffer.append(buffer, bytes_read);

    // Process complete messages
    while (protocol::MessageFramer::isCompleteMessage(message_buffer)) {
      std::string framed_message = protocol::MessageFramer::unframeMessage(message_buffer);
      std::string request_json = protocol::MessageFramer::unframeMessage(framed_message);

      try {
        auto request = protocol::deserializeRequest(request_json);
        std::string response_json = request_handler_(request_json);

        // Frame and send response
        std::string framed_response = protocol::MessageFramer::frameMessage(response_json);
        std::string final_message = protocol::MessageFramer::frameMessage(framed_response);

        ssize_t bytes_written = write(client_socket, final_message.c_str(), final_message.size());
        if (bytes_written < 0) {
          std::cerr << "Error writing to client " << client_addr << std::endl;
          break;
        }
      } catch (const std::exception& e) {
        std::cerr << "Error processing request from " << client_addr << ": " << e.what() << std::endl;

        // Send error response
        auto error_response = protocol::Response::error(
            protocol::Status::ERROR, "Invalid request format", 0);
        std::string error_json = protocol::serializeResponse(error_response);
        std::string framed_error = protocol::MessageFramer::frameMessage(error_json);
        std::string final_error = protocol::MessageFramer::frameMessage(framed_error);

        write(client_socket, final_error.c_str(), final_error.size());
      }
    }
  }

  close(client_socket);
  std::cout << "Closed connection from " << client_addr << std::endl;

  // Remove from active connections
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    client_threads_.erase(client_socket);
  }
}

size_t TCPServer::getConnectionCount() const {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  return client_threads_.size();
}

}  // namespace network
}  // namespace banking
