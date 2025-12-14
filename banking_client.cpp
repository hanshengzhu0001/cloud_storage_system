#include "include/network/tcp_client.hpp"
#include "include/network/protocol.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace banking::network;

/**
 * Simple banking client application demonstrating server interaction.
 */
class BankingClient {
 public:
  BankingClient(const std::string& host, int port)
      : client_(host, port), session_token_(""), client_id_("client_123") {}

  bool connect() {
    return client_.connect();
  }

  void disconnect() {
    client_.disconnect();
  }

  bool authenticate() {
    auto request = protocol::Request::authenticate(1000, "user123", "password123");
    request.client_id = client_id_;

    try {
      std::string response_json = client_.sendRequest(protocol::serializeRequest(request));
      auto response = protocol::deserializeResponse(response_json);

      if (response.status == protocol::Status::SUCCESS) {
        session_token_ = response.payload["session_token"];
        std::cout << "Authentication successful! Session: " << session_token_ << std::endl;
        return true;
      } else {
        std::cout << "Authentication failed: " << response.message << std::endl;
        return false;
      }
    } catch (const std::exception& e) {
      std::cout << "Authentication error: " << e.what() << std::endl;
      return false;
    }
  }

  void createAccount(const std::string& account_id) {
    auto request = protocol::Request::createAccount(getTimestamp(), client_id_, session_token_, account_id);
    sendRequest(request, "Create Account");
  }

  void deposit(const std::string& account_id, int amount) {
    auto request = protocol::Request::deposit(getTimestamp(), client_id_, session_token_, account_id, amount);
    sendRequest(request, "Deposit");
  }

  void transfer(const std::string& from_account, const std::string& to_account, int amount) {
    auto request = protocol::Request::transfer(getTimestamp(), client_id_, session_token_,
                                             from_account, to_account, amount);
    sendRequest(request, "Transfer");
  }

  void getBalance(const std::string& account_id) {
    auto request = protocol::Request::getBalance(getTimestamp(), client_id_, session_token_,
                                                account_id, getTimestamp());
    sendRequest(request, "Get Balance");
  }

  void topSpenders(int n) {
    auto request = protocol::Request::topSpenders(getTimestamp(), client_id_, session_token_, n);
    sendRequest(request, "Top Spenders");
  }

  void sendHeartbeat() {
    auto request = protocol::Request::heartbeat(getTimestamp(), client_id_);
    sendRequest(request, "Heartbeat");
  }

 private:
  void sendRequest(const protocol::Request& request, const std::string& operation_name) {
    try {
      std::string request_json = protocol::serializeRequest(request);
      std::string response_json = client_.sendRequest(request_json);
      auto response = protocol::deserializeResponse(response_json);

      std::cout << operation_name << " - Status: "
                << (response.status == protocol::Status::SUCCESS ? "SUCCESS" : "ERROR")
                << " - Message: " << response.message << std::endl;

      if (!response.payload.empty()) {
        std::cout << "Response data: " << response.payload.dump(2) << std::endl;
      }

    } catch (const std::exception& e) {
      std::cout << operation_name << " failed: " << e.what() << std::endl;
    }
  }

  int getTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
  }

  TCPClient client_;
  std::string session_token_;
  std::string client_id_;
};

void demonstrateBankingOperations(BankingClient& client) {
  std::cout << "\n=== Banking System Demonstration ===\n" << std::endl;

  // Authenticate
  if (!client.authenticate()) {
    std::cerr << "Failed to authenticate. Exiting." << std::endl;
    return;
  }

  // Create accounts
  std::cout << "\n--- Creating Accounts ---" << std::endl;
  client.createAccount("alice_account");
  client.createAccount("bob_account");

  // Deposits
  std::cout << "\n--- Making Deposits ---" << std::endl;
  client.deposit("alice_account", 1000);
  client.deposit("bob_account", 500);

  // Check balances
  std::cout << "\n--- Checking Balances ---" << std::endl;
  client.getBalance("alice_account");
  client.getBalance("bob_account");

  // Transfers
  std::cout << "\n--- Making Transfers ---" << std::endl;
  client.transfer("alice_account", "bob_account", 200);
  client.transfer("bob_account", "alice_account", 100);

  // Check balances after transfers
  std::cout << "\n--- Balances After Transfers ---" << std::endl;
  client.getBalance("alice_account");
  client.getBalance("bob_account");

  // Top spenders
  std::cout << "\n--- Top Spenders ---" << std::endl;
  client.topSpenders(5);

  // Send heartbeat
  std::cout << "\n--- Heartbeat ---" << std::endl;
  client.sendHeartbeat();

  std::cout << "\n=== Demonstration Complete ===\n" << std::endl;
}

int main(int argc, char* argv[]) {
  std::string host = "localhost";
  int port = 8080;

  if (argc >= 2) host = argv[1];
  if (argc >= 3) port = std::stoi(argv[2]);

  std::cout << "Connecting to banking server at " << host << ":" << port << std::endl;

  BankingClient client(host, port);

  if (!client.connect()) {
    std::cerr << "Failed to connect to server" << std::endl;
    return 1;
  }

  demonstrateBankingOperations(client);

  // Simulate some concurrent activity
  std::cout << "Simulating concurrent activity..." << std::endl;
  std::vector<std::thread> threads;

  for (int i = 0; i < 3; ++i) {
    threads.emplace_back([&client, i]() {
      for (int j = 0; j < 5; ++j) {
        client.sendHeartbeat();
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + i * 50));
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  client.disconnect();
  std::cout << "Disconnected from server" << std::endl;

  return 0;
}
