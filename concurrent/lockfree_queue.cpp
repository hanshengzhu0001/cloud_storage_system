#include "lockfree_queue.hpp"

#include <chrono>

namespace banking {
namespace concurrent {

template<typename T>
LockFreeQueue<T>::LockFreeQueue() : size_(0) {
  // Create dummy node
  Node* dummy = new Node(T{});
  head_.store(dummy);
  tail_.store(dummy);
}

template<typename T>
LockFreeQueue<T>::~LockFreeQueue() {
  clear();
  Node* dummy = head_.load();
  if (dummy) {
    delete dummy;
  }
}

template<typename T>
void LockFreeQueue<T>::enqueue(T item) {
  Node* new_node = new Node(std::move(item));
  Node* old_tail = tail_.exchange(new_node);

  // Link the old tail to the new node
  old_tail->next.store(new_node);
  size_.fetch_add(1);
}

template<typename T>
std::optional<T> LockFreeQueue<T>::dequeue() {
  Node* head = head_.load();
  Node* next = head->next.load();

  if (next == nullptr) {
    return std::nullopt;  // Queue is empty
  }

  // Try to advance head
  if (head_.compare_exchange_strong(head, next)) {
    T result = std::move(next->data);
    delete head;
    size_.fetch_sub(1);
    return result;
  }

  return std::nullopt;  // Another thread got there first
}

template<typename T>
bool LockFreeQueue<T>::empty() const {
  Node* head = head_.load();
  return head->next.load() == nullptr;
}

template<typename T>
size_t LockFreeQueue<T>::size() const {
  return size_.load();
}

template<typename T>
void LockFreeQueue<T>::clear() {
  while (!empty()) {
    dequeue();
  }
}

// Explicit template instantiations for common types
template class LockFreeQueue<std::string>;
template class LockFreeQueue<int>;

}  // namespace concurrent
}  // namespace banking
