#include "tcp_client.hpp"
#include "protocol.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>

namespace banking {
namespace network {

TCPClient::TCPClient(const std::string& host, int port)
    : host_(host),
      port_(port),
      socket_(-1),
      connected_(false),
      response_ready_(false) {
}

TCPClient::~TCPClient() {
  disconnect();
}

bool TCPClient::connect() {
  if (connected_) return true;

  // Create socket
  socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ < 0) {
    std::cerr << "Failed to create socket" << std::endl;
    return false;
  }

  // Set up server address
  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port_);

  if (inet_pton(AF_INET, host_.c_str(), &server_address.sin_addr) <= 0) {
    std::cerr << "Invalid address: " << host_ << std::endl;
    close(socket_);
    socket_ = -1;
    return false;
  }

  // Connect to server
  if (::connect(socket_, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
    std::cerr << "Failed to connect to " << host_ << ":" << port_ << std::endl;
    close(socket_);
    socket_ = -1;
    return false;
  }

  connected_ = true;
  receive_thread_ = std::make_unique<std::thread>(&TCPClient::receiveLoop, this);

  std::cout << "Connected to server at " << host_ << ":" << port_ << std::endl;
  return true;
}

void TCPClient::disconnect() {
  if (!connected_) return;

  connected_ = false;

  // Close socket to break receive loop
  if (socket_ >= 0) {
    shutdown(socket_, SHUT_RDWR);
    close(socket_);
    socket_ = -1;
  }

  // Wait for receive thread
  if (receive_thread_ && receive_thread_->joinable()) {
    receive_thread_->join();
  }

  std::cout << "Disconnected from server" << std::endl;
}

std::string TCPClient::sendRequest(const std::string& request) {
  if (!connected_) {
    throw std::runtime_error("Not connected to server");
  }

  {
    std::lock_guard<std::mutex> lock(response_mutex_);
    response_ready_ = false;
  }

  if (!sendMessage(request)) {
    throw std::runtime_error("Failed to send request");
  }

  // Wait for response
  {
    std::unique_lock<std::mutex> lock(response_mutex_);
    response_cv_.wait(lock, [this]() { return response_ready_; });
    return pending_response_;
  }
}

bool TCPClient::sendRequestAsync(const std::string& request) {
  if (!connected_) return false;
  return sendMessage(request);
}

void TCPClient::receiveLoop() {
  char buffer[4096];
  std::string message_buffer;

  while (connected_) {
    ssize_t bytes_read = read(socket_, buffer, sizeof(buffer));

    if (bytes_read <= 0) {
      if (bytes_read < 0 && connected_) {
        std::cerr << "Error reading from server" << std::endl;
      }
      break;
    }

    message_buffer.append(buffer, bytes_read);

    // Process complete messages
    while (protocol::MessageFramer::isCompleteMessage(message_buffer)) {
      std::string framed_message = protocol::MessageFramer::unframeMessage(message_buffer);
      std::string response_json = protocol::MessageFramer::unframeMessage(framed_message);

      // Notify waiting thread
      {
        std::lock_guard<std::mutex> lock(response_mutex_);
        pending_response_ = response_json;
        response_ready_ = true;
      }
      response_cv_.notify_one();
    }
  }

  connected_ = false;
}

bool TCPClient::sendMessage(const std::string& message) {
  std::lock_guard<std::mutex> lock(socket_mutex_);

  if (!connected_ || socket_ < 0) return false;

  // Frame the message
  std::string framed_message = protocol::MessageFramer::frameMessage(message);
  std::string final_message = protocol::MessageFramer::frameMessage(framed_message);

  ssize_t bytes_written = write(socket_, final_message.c_str(), final_message.size());

  if (bytes_written < 0) {
    std::cerr << "Failed to send message" << std::endl;
    connected_ = false;
    return false;
  }

  return true;
}

}  // namespace network
}  // namespace banking
