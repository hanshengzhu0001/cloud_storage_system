#ifndef BANKING_SERVER_HPP_
#define BANKING_SERVER_HPP_

#include "network/tcp_server.hpp"
#include "banking_system_thread_safe.hpp"
#include "concurrent/transaction_processor.hpp"
#include "ai/fraud_detection_agent.hpp"

#include <memory>
#include <unordered_map>
#include <string>

namespace banking {

/**
 * Main banking server that integrates all components.
 * Handles client connections, processes transactions, and coordinates fraud detection.
 */
class BankingServer {
 public:
  BankingServer(int port,
                size_t num_worker_threads = 4,
                size_t analysis_window_seconds = 3600);
  ~BankingServer();

  // Non-copyable
  BankingServer(const BankingServer&) = delete;
  BankingServer& operator=(const BankingServer&) = delete;

  /**
   * Start the banking server.
   */
  bool start();

  /**
   * Stop the banking server.
   */
  void stop();

  /**
   * Get server statistics.
   */
  struct Stats {
    bool is_running;
    size_t active_connections;
    concurrent::TransactionProcessor::Stats transaction_stats;
    ai::FraudDetectionAgent::Stats fraud_stats;
  };
  Stats getStats() const;

  /**
   * Get server port.
   */
  int getPort() const { return port_; }

 private:
  /**
   * Handle incoming client requests.
   */
  std::string handleRequest(const std::string& request_json);

  /**
   * Handle fraud detection alerts.
   */
  void handleFraudAlert(const ai::TransactionData& transaction,
                       const ai::FraudResult& result);

  /**
   * Extract transaction data from request for fraud analysis.
   */
  ai::TransactionData extractTransactionData(const network::protocol::Request& request);

  int port_;

  // Core components
  std::unique_ptr<BankingSystemThreadSafe> banking_system_;
  std::unique_ptr<concurrent::TransactionProcessor> transaction_processor_;
  std::unique_ptr<ai::FraudDetectionAgent> fraud_agent_;
  std::unique_ptr<network::TCPServer> tcp_server_;

  // Session management (simplified - in production, use proper JWT/session management)
  std::unordered_map<std::string, std::string> active_sessions_;  // client_id -> session_token
  mutable std::shared_mutex sessions_mutex_;
};

}  // namespace banking

#endif  // BANKING_SERVER_HPP_
