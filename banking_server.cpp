#include "banking_server.hpp"

#include "banking_core_impl.hpp"
#include "network/protocol.hpp"
#include "ai/fraud_detection_agent.hpp"

#include <iostream>
#include <chrono>

namespace banking {

BankingServer::BankingServer(int port, size_t num_worker_threads, size_t analysis_window_seconds)
    : port_(port) {
  // Initialize components
  auto banking_impl = std::make_unique<BankingSystemImpl>();
  banking_system_ = std::make_unique<BankingSystemThreadSafe>(std::move(banking_impl));

  transaction_processor_ = std::make_unique<concurrent::TransactionProcessor>(
      banking_system_.get(), num_worker_threads);

  fraud_agent_ = std::make_unique<ai::FraudDetectionAgent>(
      analysis_window_seconds);

  // Set up fraud alert handling
  fraud_agent_->setAlertCallback(
      [this](const ai::TransactionData& tx, const ai::FraudResult& result) {
        handleFraudAlert(tx, result);
      });

  // Create TCP server with request handler
  tcp_server_ = std::make_unique<network::TCPServer>(
      port_, [this](const std::string& request) {
        return handleRequest(request);
      });
}

BankingServer::~BankingServer() {
  stop();
}

bool BankingServer::start() {
  std::cout << "Starting Banking Server on port " << port_ << "..." << std::endl;

  // Start components in order
  if (!fraud_agent_->start()) {
    std::cerr << "Failed to start fraud detection agent" << std::endl;
    return false;
  }

  if (!transaction_processor_->start()) {
    std::cerr << "Failed to start transaction processor" << std::endl;
    fraud_agent_->stop();
    return false;
  }

  if (!tcp_server_->start()) {
    std::cerr << "Failed to start TCP server" << std::endl;
    transaction_processor_->stop();
    fraud_agent_->stop();
    return false;
  }

  std::cout << "Banking Server started successfully!" << std::endl;
  std::cout << "- TCP Server listening on port " << port_ << std::endl;
  std::cout << "- Transaction processor with " << transaction_processor_.get()->getStats().transactions_processed << " workers" << std::endl;
  std::cout << "- Fraud detection agent active" << std::endl;

  return true;
}

void BankingServer::stop() {
  std::cout << "Stopping Banking Server..." << std::endl;

  // Stop components in reverse order
  if (tcp_server_) {
    tcp_server_->stop();
  }

  if (transaction_processor_) {
    transaction_processor_->stop();
  }

  if (fraud_agent_) {
    fraud_agent_->stop();
  }

  std::cout << "Banking Server stopped" << std::endl;
}

BankingServer::Stats BankingServer::getStats() const {
  Stats stats;
  stats.is_running = tcp_server_ && tcp_server_->isRunning();
  stats.active_connections = tcp_server_ ? tcp_server_->getConnectionCount() : 0;
  stats.transaction_stats = transaction_processor_->getStats();
  stats.fraud_stats = fraud_agent_->getStats();
  return stats;
}

std::string BankingServer::handleRequest(const std::string& request_json) {
  try {
    auto request = network::protocol::deserializeRequest(request_json);

    // Basic authentication check (simplified)
    if (request.type != network::protocol::MessageType::AUTHENTICATE &&
        request.type != network::protocol::MessageType::HEARTBEAT) {
      {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
        auto it = active_sessions_.find(request.client_id);
        if (it == active_sessions_.end() || it->second != request.session_token) {
          auto error_response = network::protocol::Response::error(
              network::protocol::Status::UNAUTHORIZED, "Invalid session", request.timestamp);
          return network::protocol::serializeResponse(error_response);
        }
      }
    }

    // Handle authentication
    if (request.type == network::protocol::MessageType::AUTHENTICATE) {
      // Simplified authentication - in production, verify credentials
      std::string session_token = "session_" + request.client_id + "_" +
                                 std::to_string(request.timestamp);

      {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
        active_sessions_[request.client_id] = session_token;
      }

      auto response = network::protocol::Response::authenticated(session_token, request.timestamp);
      return network::protocol::serializeResponse(response);
    }

    // Handle heartbeat
    if (request.type == network::protocol::MessageType::HEARTBEAT) {
      auto response = network::protocol::Response::success("Heartbeat acknowledged",
                                                          request.timestamp);
      return network::protocol::serializeResponse(response);
    }

    // Submit transaction for processing
    transaction_processor_->submitTransaction(request_json);

    // Submit to fraud detection if it's a financial transaction
    if (request.type == network::protocol::MessageType::TRANSFER ||
        request.type == network::protocol::MessageType::DEPOSIT ||
        request.type == network::protocol::MessageType::SCHEDULE_PAYMENT) {

      ai::TransactionData tx_data = extractTransactionData(request);
      fraud_agent_->submitTransaction(tx_data);
    }

    // For synchronous operations, wait for result (simplified - in production, use async)
    // For now, return acknowledgment
    auto response = network::protocol::Response::success("Request queued for processing",
                                                        request.timestamp);
    return network::protocol::serializeResponse(response);

  } catch (const std::exception& e) {
    std::cerr << "Error handling request: " << e.what() << std::endl;
    auto error_response = network::protocol::Response::error(
        network::protocol::Status::ERROR, "Request processing failed", 0);
    return network::protocol::serializeResponse(error_response);
  }
}

void BankingServer::handleFraudAlert(const ai::TransactionData& transaction,
                                    const ai::FraudResult& result) {
  std::cout << "FRAUD ALERT: Account " << transaction.account_id
            << " - Risk Score: " << result.risk_score
            << " - Recommendation: " << result.recommendation << std::endl;

  if (!result.risk_factors.empty()) {
    std::cout << "Risk Factors: ";
    for (const auto& factor : result.risk_factors) {
      std::cout << factor << "; ";
    }
    std::cout << std::endl;
  }

  // In production, this would trigger additional actions:
  // - Send alerts to compliance team
  // - Freeze account if risk is very high
  // - Log to security system
  // - Update risk models
}

ai::TransactionData BankingServer::extractTransactionData(
    const network::protocol::Request& request) {

  ai::TransactionData tx_data(request.client_id, "", 0, request.timestamp);

  switch (request.type) {
    case network::protocol::MessageType::DEPOSIT:
      tx_data.transaction_type = "DEPOSIT";
      tx_data.amount = request.payload["amount"];
      break;

    case network::protocol::MessageType::TRANSFER:
      tx_data.transaction_type = "TRANSFER";
      tx_data.amount = request.payload["amount"];
      break;

    case network::protocol::MessageType::SCHEDULE_PAYMENT:
      tx_data.transaction_type = "PAYMENT";
      tx_data.amount = request.payload["amount"];
      break;

    default:
      tx_data.transaction_type = "UNKNOWN";
      break;
  }

  // Add metadata for fraud analysis
  tx_data.metadata["operation"] = std::to_string(static_cast<int>(request.type));
  if (request.payload.contains("account_id")) {
    tx_data.account_id = request.payload["account_id"];
  }

  return tx_data;
}

}  // namespace banking
