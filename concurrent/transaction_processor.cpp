#include "transaction_processor.hpp"
#include "../network/protocol.hpp"

#include <chrono>
#include <iostream>

namespace banking {
namespace concurrent {

TransactionProcessor::TransactionProcessor(BankingSystem* banking_system,
                                         size_t num_worker_threads,
                                         size_t batch_size)
    : banking_system_(banking_system),
      num_workers_(num_worker_threads),
      batch_size_(batch_size),
      running_(false),
      transactions_processed_(0),
      total_processing_time_us_(0) {
}

TransactionProcessor::~TransactionProcessor() {
  stop();
}

bool TransactionProcessor::start() {
  if (running_) return true;

  running_ = true;

  // Start worker threads
  for (size_t i = 0; i < num_workers_; ++i) {
    worker_threads_.emplace_back(
        std::make_unique<std::thread>(&TransactionProcessor::workerThread, this));
  }

  std::cout << "Transaction processor started with " << num_workers_ << " worker threads" << std::endl;
  return true;
}

void TransactionProcessor::stop() {
  if (!running_) return;

  running_ = false;

  // Wake up workers by submitting dummy transactions
  for (size_t i = 0; i < num_workers_; ++i) {
    submitTransaction("");  // Empty string signals shutdown
  }

  // Wait for all workers to finish
  for (auto& thread : worker_threads_) {
    if (thread && thread->joinable()) {
      thread->join();
    }
  }
  worker_threads_.clear();

  std::cout << "Transaction processor stopped" << std::endl;
}

void TransactionProcessor::submitTransaction(const std::string& transaction_json) {
  transaction_queue_.enqueue(transaction_json);
}

void TransactionProcessor::setTransactionCallback(TransactionCallback callback) {
  callback_ = std::move(callback);
}

size_t TransactionProcessor::getQueueSize() const {
  return transaction_queue_.size();
}

TransactionProcessor::Stats TransactionProcessor::getStats() const {
  Stats stats;
  stats.transactions_processed = transactions_processed_.load();
  stats.transactions_queued = transaction_queue_.size();

  size_t total_time = total_processing_time_us_.load();
  if (stats.transactions_processed > 0) {
    stats.avg_processing_time_ms = static_cast<double>(total_time) /
                                  stats.transactions_processed / 1000.0;
  } else {
    stats.avg_processing_time_ms = 0.0;
  }

  // Simple throughput calculation (could be improved with time windows)
  stats.throughput_tps = stats.transactions_processed / 1.0;  // per second approximation

  return stats;
}

void TransactionProcessor::workerThread() {
  while (running_) {
    auto transaction_opt = transaction_queue_.dequeue();
    if (!transaction_opt.has_value()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    std::string transaction_json = *transaction_opt;

    // Empty transaction signals shutdown
    if (transaction_json.empty()) {
      break;
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    processTransaction(transaction_json);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);

    transactions_processed_.fetch_add(1);
    total_processing_time_us_.fetch_add(duration.count());
  }
}

void TransactionProcessor::processTransaction(const std::string& transaction_json) {
  try {
    // Parse the transaction
    auto request = protocol::deserializeRequest(transaction_json);

    // Process based on request type
    protocol::Response response = protocol::Response::error(
        protocol::Status::ERROR, "Unknown operation", request.timestamp);

    switch (request.type) {
      case protocol::MessageType::CREATE_ACCOUNT: {
        bool success = banking_system_->CreateAccount(
            request.timestamp, request.payload["account_id"]);
        if (success) {
          response = protocol::Response::accountCreated(
              request.payload["account_id"], request.timestamp);
        } else {
          response = protocol::Response::error(
              protocol::Status::ERROR, "Account creation failed", request.timestamp);
        }
        break;
      }

      case protocol::MessageType::DEPOSIT: {
        auto result = banking_system_->Deposit(
            request.timestamp, request.payload["account_id"], request.payload["amount"]);
        if (result.has_value()) {
          response = protocol::Response::depositResult(*result, request.timestamp);
        } else {
          response = protocol::Response::error(
              protocol::Status::ACCOUNT_NOT_FOUND, "Account not found", request.timestamp);
        }
        break;
      }

      case protocol::MessageType::TRANSFER: {
        auto result = banking_system_->Transfer(
            request.timestamp,
            request.payload["source_account"],
            request.payload["target_account"],
            request.payload["amount"]);
        if (result.has_value()) {
          response = protocol::Response::transferResult(*result, request.timestamp);
        } else {
          response = protocol::Response::error(
              protocol::Status::INSUFFICIENT_FUNDS, "Transfer failed", request.timestamp);
        }
        break;
      }

      case protocol::MessageType::GET_BALANCE: {
        auto result = banking_system_->GetBalance(
            request.timestamp, request.payload["account_id"], request.payload["time_at"]);
        if (result.has_value()) {
          response = protocol::Response::balanceResult(*result, request.timestamp);
        } else {
          response = protocol::Response::error(
              protocol::Status::ACCOUNT_NOT_FOUND, "Account not found", request.timestamp);
        }
        break;
      }

      case protocol::MessageType::TOP_SPENDERS: {
        auto spenders = banking_system_->TopSpenders(
            request.timestamp, request.payload["n"]);
        response = protocol::Response::topSpendersResult(spenders, request.timestamp);
        break;
      }

      case protocol::MessageType::SCHEDULE_PAYMENT: {
        auto payment_id = banking_system_->SchedulePayment(
            request.timestamp, request.payload["account_id"],
            request.payload["amount"], request.payload["delay"]);
        if (payment_id.has_value()) {
          response = protocol::Response::paymentScheduled(*payment_id, request.timestamp);
        } else {
          response = protocol::Response::error(
              protocol::Status::ACCOUNT_NOT_FOUND, "Payment scheduling failed", request.timestamp);
        }
        break;
      }

      case protocol::MessageType::CANCEL_PAYMENT: {
        bool success = banking_system_->CancelPayment(
            request.timestamp, request.payload["account_id"], request.payload["payment_id"]);
        if (success) {
          response = protocol::Response::paymentCancelled(request.timestamp);
        } else {
          response = protocol::Response::error(
              protocol::Status::ERROR, "Payment cancellation failed", request.timestamp);
        }
        break;
      }

      case protocol::MessageType::MERGE_ACCOUNTS: {
        bool success = banking_system_->MergeAccounts(
            request.timestamp, request.payload["account_id_1"], request.payload["account_id_2"]);
        if (success) {
          response = protocol::Response::accountsMerged(request.timestamp);
        } else {
          response = protocol::Response::error(
              protocol::Status::ERROR, "Account merge failed", request.timestamp);
        }
        break;
      }

      default:
        response = protocol::Response::error(
            protocol::Status::INVALID_REQUEST, "Unsupported operation", request.timestamp);
        break;
    }

    // Call callback if set
    if (callback_) {
      callback_(protocol::serializeResponse(response));
    }

  } catch (const std::exception& e) {
    std::cerr << "Error processing transaction: " << e.what() << std::endl;

    protocol::Response error_response = protocol::Response::error(
        protocol::Status::ERROR, "Processing error", 0);

    if (callback_) {
      callback_(protocol::serializeResponse(error_response));
    }
  }
}

}  // namespace concurrent
}  // namespace banking
