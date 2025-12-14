#ifndef TRANSACTION_PROCESSOR_HPP_
#define TRANSACTION_PROCESSOR_HPP_

#include "lockfree_queue.hpp"
#include "banking_system.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace banking {
namespace concurrent {

/**
 * High-throughput transaction processor using lock-free queues.
 * Processes banking transactions concurrently with multiple worker threads.
 */
class TransactionProcessor {
 public:
  using TransactionCallback = std::function<void(const std::string&)>;

  TransactionProcessor(BankingSystem* banking_system,
                      size_t num_worker_threads = 4,
                      size_t batch_size = 100);
  ~TransactionProcessor();

  // Non-copyable
  TransactionProcessor(const TransactionProcessor&) = delete;
  TransactionProcessor& operator=(const TransactionProcessor&) = delete;

  /**
   * Start the transaction processor.
   */
  bool start();

  /**
   * Stop the transaction processor.
   */
  void stop();

  /**
   * Submit a transaction for processing.
   */
  void submitTransaction(const std::string& transaction_json);

  /**
   * Set callback for processed transactions.
   */
  void setTransactionCallback(TransactionCallback callback);

  /**
   * Get current queue size.
   */
  size_t getQueueSize() const;

  /**
   * Get processing statistics.
   */
  struct Stats {
    size_t transactions_processed;
    size_t transactions_queued;
    double avg_processing_time_ms;
    double throughput_tps;
  };
  Stats getStats() const;

 private:
  void workerThread();
  void processTransaction(const std::string& transaction_json);

  BankingSystem* banking_system_;
  size_t num_workers_;
  size_t batch_size_;

  LockFreeQueue<std::string> transaction_queue_;
  std::vector<std::unique_ptr<std::thread>> worker_threads_;
  std::atomic<bool> running_;

  TransactionCallback callback_;

  // Statistics
  std::atomic<size_t> transactions_processed_;
  std::atomic<size_t> total_processing_time_us_;
  mutable std::mutex stats_mutex_;
};

}  // namespace concurrent
}  // namespace banking

#endif  // TRANSACTION_PROCESSOR_HPP_
