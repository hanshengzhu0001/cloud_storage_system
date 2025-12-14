#ifndef LOCKFREE_QUEUE_HPP_
#define LOCKFREE_QUEUE_HPP_

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace banking {
namespace concurrent {

/**
 * Lock-free Multiple Producer Single Consumer (MPSC) queue.
 * Uses atomic operations for thread-safe enqueue/dequeue without locks.
 * Optimized for high-throughput transaction processing.
 */
template<typename T>
class LockFreeQueue {
 private:
  struct Node {
    T data;
    std::atomic<Node*> next;

    Node(T value) : data(std::move(value)), next(nullptr) {}
  };

 public:
  LockFreeQueue();
  ~LockFreeQueue();

  // Non-copyable
  LockFreeQueue(const LockFreeQueue&) = delete;
  LockFreeQueue& operator=(const LockFreeQueue&) = delete;

  /**
   * Enqueue an item (thread-safe for multiple producers).
   */
  void enqueue(T item);

  /**
   * Dequeue an item (single consumer only).
   * Returns empty optional if queue is empty.
   */
  std::optional<T> dequeue();

  /**
   * Check if queue is empty (not thread-safe).
   */
  bool empty() const;

  /**
   * Get approximate size (not thread-safe, for monitoring only).
   */
  size_t size() const;

  /**
   * Clear all elements from the queue.
   */
  void clear();

 private:
  std::atomic<Node*> head_;
  std::atomic<Node*> tail_;
  std::atomic<size_t> size_;
};

/**
 * Transaction batch for efficient processing.
 */
template<typename T>
struct TransactionBatch {
  std::vector<T> transactions;
  size_t batch_id;
  std::chrono::steady_clock::time_point enqueue_time;

  TransactionBatch(size_t id) : batch_id(id), enqueue_time(std::chrono::steady_clock::now()) {}
};

}  // namespace concurrent
}  // namespace banking

#endif  // LOCKFREE_QUEUE_HPP_
