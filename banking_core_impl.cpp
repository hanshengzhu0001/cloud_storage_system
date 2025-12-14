#include "banking_system_impl.hpp"

#include <algorithm>
#include <string>

// Internal: process scheduled payments that are due at or before the given timestamp.
void BankingSystemImpl::processDuePayments(int timestamp) {
  auto it = dueTimeToPaymentIds_.begin();
  while (it != dueTimeToPaymentIds_.end() && it->first <= timestamp) {
    std::vector<std::string> paymentIdsAtThisTime = std::move(it->second);
    auto toErase = it++;
    dueTimeToPaymentIds_.erase(toErase);

    for (const std::string& paymentId : paymentIdsAtThisTime) {
      auto itInfo = paymentById_.find(paymentId);
      if (itInfo == paymentById_.end()) {
        continue;
      }
      PaymentInfo& info = itInfo->second;
      if (info.canceled) {
        continue;
      }

      auto itAcc = accountBalances_.find(info.accountId);
      if (itAcc == accountBalances_.end()) {
        continue;
      }
      if (itAcc->second >= info.amount) {
        itAcc->second -= info.amount;
        balanceEvents_[info.accountId].emplace_back(info.dueTimestamp, -info.amount);
        accountOutgoing_[info.accountId] += info.amount;
      }
      info.processed = true;
    }
  }
}

// Resolve current owner at a given time by following merges with mergeTimestamp <= time_at.
std::string BankingSystemImpl::rootAtTime(const std::string& account_id, int time_at) const {
  auto it = mergedInto_.find(account_id);
  if (it == mergedInto_.end()) return account_id;
  const auto& [parent, mergeTs] = it->second;
  if (mergeTs <= time_at) {
    return rootAtTime(parent, time_at);
  }
  return account_id;
}

// Creates a new account with zero balance if it doesn't exist yet.
bool BankingSystemImpl::CreateAccount(int timestamp, const std::string& account_id) {
  processDuePayments(timestamp);

  if (accountBalances_.find(account_id) != accountBalances_.end()) {
    return false;
  }
  accountBalances_[account_id] = 0;
  // Level 4: record creation time (multiple lifetimes supported)
  creationTimes_[account_id].push_back(timestamp);
  accountCreationTime_[account_id] = accountCreationTime_.count(account_id) ? accountCreationTime_[account_id] : timestamp;
  balanceEvents_[account_id].emplace_back(timestamp, 0);  // mark creation point
  // Clear prior merge edge for new lifetime
  mergedInto_.erase(account_id);
  return true;
}

// Deposits the given amount and returns the new balance; nullopt if account is missing.
std::optional<int> BankingSystemImpl::Deposit(int timestamp, const std::string& account_id, int amount) {
  processDuePayments(timestamp);

  auto it = accountBalances_.find(account_id);
  if (it == accountBalances_.end()) {
    return std::nullopt;
  }
  it->second += amount;
  balanceEvents_[account_id].emplace_back(timestamp, amount);
  return it->second;
}

// Transfers funds between two different existing accounts if the source has enough money.
std::optional<int> BankingSystemImpl::Transfer(int timestamp, const std::string& source_account_id, const std::string& target_account_id, int amount) {
  processDuePayments(timestamp);

  if (source_account_id == target_account_id) {
    return std::nullopt;
  }

  auto itSource = accountBalances_.find(source_account_id);
  auto itTarget = accountBalances_.find(target_account_id);
  if (itSource == accountBalances_.end() || itTarget == accountBalances_.end()) {
    return std::nullopt;
  }
  if (itSource->second < amount) {
    return std::nullopt;
  }

  itSource->second -= amount;
  itTarget->second += amount;
  balanceEvents_[source_account_id].emplace_back(timestamp, -amount);
  balanceEvents_[target_account_id].emplace_back(timestamp, amount);

  accountOutgoing_[source_account_id] += amount;

  return itSource->second;
}

std::vector<std::string> BankingSystemImpl::TopSpenders(int timestamp, int n) {
  processDuePayments(timestamp);

  std::vector<std::pair<std::string, int>> accountAndOutgoing;
  accountAndOutgoing.reserve(accountBalances_.size());

  for (const auto& [accountId, _] : accountBalances_) {
    int outgoing = 0;
    auto it = accountOutgoing_.find(accountId);
    if (it != accountOutgoing_.end()) {
      outgoing = it->second;
    }
    accountAndOutgoing.emplace_back(accountId, outgoing);
  }

  std::sort(accountAndOutgoing.begin(), accountAndOutgoing.end(), [](const auto& a, const auto& b) {
    if (a.second != b.second) return a.second > b.second;
    return a.first < b.first;
  });

  if (n < 0) n = 0;
  const int limit = std::min<int>(n, static_cast<int>(accountAndOutgoing.size()));

  std::vector<std::string> result;
  result.reserve(limit);
  for (int i = 0; i < limit; ++i) {
    const auto& [id, outgoing] = accountAndOutgoing[i];
    result.emplace_back(id + "(" + std::to_string(outgoing) + ")");
  }
  return result;
}

std::optional<std::string> BankingSystemImpl::SchedulePayment(int timestamp, const std::string& account_id, int amount, int delay) {
  processDuePayments(timestamp);

  if (accountBalances_.find(account_id) == accountBalances_.end()) {
    return std::nullopt;
  }

  const int dueTime = timestamp + delay;
  const int ordinal = nextPaymentOrdinal_++;
  const std::string paymentId = std::string("payment") + std::to_string(ordinal);

  PaymentInfo info{account_id, amount, dueTime, false, false, ordinal};
  paymentById_.emplace(paymentId, std::move(info));
  dueTimeToPaymentIds_[dueTime].push_back(paymentId);

  return paymentId;
}

bool BankingSystemImpl::CancelPayment(int timestamp, const std::string& account_id, const std::string& payment_id) {
  processDuePayments(timestamp);

  auto it = paymentById_.find(payment_id);
  if (it == paymentById_.end()) {
    return false;
  }
  PaymentInfo& info = it->second;
  if (info.canceled) {
    return false;
  }
  if (info.processed) {
    return false;
  }
  if (info.accountId != account_id) {
    return false;
  }

  info.canceled = true;
  return true;
}

bool BankingSystemImpl::MergeAccounts(int timestamp, const std::string& account_id_1, const std::string& account_id_2) {
  processDuePayments(timestamp);

  if (account_id_1 == account_id_2) return false;
  auto it1 = accountBalances_.find(account_id_1);
  auto it2 = accountBalances_.find(account_id_2);
  if (it1 == accountBalances_.end() || it2 == accountBalances_.end()) return false;

  // Move funds
  it1->second += it2->second;
  balanceEvents_[account_id_1].emplace_back(timestamp, it2->second);
  balanceEvents_[account_id_2].emplace_back(timestamp, -it2->second);
  it2->second = 0;

  // Sum outgoing
  accountOutgoing_[account_id_1] += accountOutgoing_[account_id_2];
  accountOutgoing_.erase(account_id_2);

  // Reassign scheduled payments from account2 to account1
  for (auto& kv : paymentById_) {
    PaymentInfo& info = kv.second;
    if (!info.processed && !info.canceled && info.accountId == account_id_2) {
      info.accountId = account_id_1;
    }
  }

  // Record merge edge for historical GetBalance
  mergedInto_[account_id_2] = {account_id_1, timestamp};

  // Remove account2 from active maps
  accountBalances_.erase(account_id_2);

  return true;
}

std::optional<int> BankingSystemImpl::GetBalance(int timestamp, const std::string& account_id, int time_at) {
  processDuePayments(timestamp);

  // Resolve to owner at time_at
  const std::string owner = rootAtTime(account_id, time_at);
  // If merged into another account by or at time_at, it's considered non-existent then
  // (Use direct merge timestamp check to avoid misclassifying pre-merge times)
  auto itMerged = mergedInto_.find(account_id);
  if (itMerged != mergedInto_.end() && itMerged->second.second < time_at) {
    return std::nullopt;
  }

  // Ensure the account existed at time_at
  auto itCreated = accountCreationTime_.find(account_id);
  if (itCreated == accountCreationTime_.end() || itCreated->second > time_at) {
    return std::nullopt;
  }

  // Sum deltas for this account up to time_at (history is recorded via balanceEvents_)
  int sum = 0;
  auto itEvents = balanceEvents_.find(account_id);
  if (itEvents != balanceEvents_.end()) {
    for (const auto& [ts, delta] : itEvents->second) {
      if (ts <= time_at) sum += delta;
    }
  }
  return sum;
}