#include "banking_system_thread_safe.hpp"

#include <algorithm>

namespace banking {

BankingSystemThreadSafe::BankingSystemThreadSafe(std::unique_ptr<BankingSystem> impl)
    : impl_(std::move(impl)) {
}

bool BankingSystemThreadSafe::CreateAccount(int timestamp, const std::string& account_id) {
  // Global lock for account creation to ensure uniqueness
  std::unique_lock<std::shared_mutex> global_lock(global_mutex_);
  return impl_->CreateAccount(timestamp, account_id);
}

std::optional<int> BankingSystemThreadSafe::Deposit(int timestamp,
                                                   const std::string& account_id,
                                                   int amount) {
  std::unique_lock<std::shared_mutex> account_lock(getAccountMutex(account_id));
  return impl_->Deposit(timestamp, account_id, amount);
}

std::optional<int> BankingSystemThreadSafe::Transfer(int timestamp,
                                                    const std::string& source_account_id,
                                                    const std::string& target_account_id,
                                                    int amount) {
  // Acquire locks in consistent order to prevent deadlocks
  std::unique_lock<std::shared_mutex> source_lock(getAccountMutex(source_account_id));
  std::unique_lock<std::shared_mutex> target_lock(getAccountMutex(target_account_id));

  // Ensure consistent locking order
  if (source_account_id > target_account_id) {
    source_lock.swap(target_lock);
  }

  return impl_->Transfer(timestamp, source_account_id, target_account_id, amount);
}

std::vector<std::string> BankingSystemThreadSafe::TopSpenders(int timestamp, int n) {
  // Global read lock for top spenders operation
  std::shared_lock<std::shared_mutex> global_lock(global_mutex_);
  return impl_->TopSpenders(timestamp, n);
}

std::optional<std::string> BankingSystemThreadSafe::SchedulePayment(int timestamp,
                                                                   const std::string& account_id,
                                                                   int amount, int delay) {
  std::unique_lock<std::shared_mutex> account_lock(getAccountMutex(account_id));
  return impl_->SchedulePayment(timestamp, account_id, amount, delay);
}

bool BankingSystemThreadSafe::CancelPayment(int timestamp,
                                           const std::string& account_id,
                                           const std::string& payment_id) {
  std::unique_lock<std::shared_mutex> account_lock(getAccountMutex(account_id));
  return impl_->CancelPayment(timestamp, account_id, payment_id);
}

bool BankingSystemThreadSafe::MergeAccounts(int timestamp,
                                           const std::string& account_id_1,
                                           const std::string& account_id_2) {
  // Acquire locks in consistent order
  std::unique_lock<std::shared_mutex> lock1(getAccountMutex(account_id_1));
  std::unique_lock<std::shared_mutex> lock2(getAccountMutex(account_id_2));

  // Ensure consistent locking order
  if (account_id_1 > account_id_2) {
    lock1.swap(lock2);
  }

  return impl_->MergeAccounts(timestamp, account_id_1, account_id_2);
}

std::optional<int> BankingSystemThreadSafe::GetBalance(int timestamp,
                                                      const std::string& account_id,
                                                      int time_at) {
  // Read lock for balance queries
  std::shared_lock<std::shared_mutex> account_lock(getAccountMutex(account_id));
  return impl_->GetBalance(timestamp, account_id, time_at);
}

std::shared_mutex& BankingSystemThreadSafe::getAccountMutex(const std::string& account_id) {
  // Double-checked locking pattern for thread-safe mutex creation
  auto it = account_mutexes_.find(account_id);
  if (it != account_mutexes_.end()) {
    return *it->second;
  }

  // Create new mutex for this account
  std::unique_lock<std::shared_mutex> global_lock(global_mutex_);
  auto& mutex_ptr = account_mutexes_[account_id];
  if (!mutex_ptr) {
    mutex_ptr = std::make_unique<std::shared_mutex>();
  }
  return *mutex_ptr;
}

}  // namespace banking
