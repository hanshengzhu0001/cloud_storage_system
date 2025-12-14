#include "../include/banking_system_thread_safe.hpp"
#include "../include/banking_core_impl.hpp"
#include "../include/concurrent/lockfree_queue.hpp"
#include "../include/ai/fraud_detection_agent.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

// Test fixture for banking system tests
class BankingSystemTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto impl = std::make_unique<BankingSystemImpl>();
    banking_system_ = std::make_unique<BankingSystemThreadSafe>(std::move(impl));
  }

  std::unique_ptr<BankingSystemThreadSafe> banking_system_;
};

// Basic account operations tests
TEST_F(BankingSystemTest, CreateAccount) {
  bool result = banking_system_->CreateAccount(1000, "acc1");
  EXPECT_TRUE(result);

  // Try to create the same account again
  result = banking_system_->CreateAccount(1001, "acc1");
  EXPECT_FALSE(result);
}

TEST_F(BankingSystemTest, Deposit) {
  banking_system_->CreateAccount(1000, "acc1");

  auto result = banking_system_->Deposit(1001, "acc1", 500);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 500);

  // Deposit again
  result = banking_system_->Deposit(1002, "acc1", 300);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 800);

  // Deposit to non-existent account
  result = banking_system_->Deposit(1003, "nonexistent", 100);
  EXPECT_FALSE(result.has_value());
}

TEST_F(BankingSystemTest, Transfer) {
  banking_system_->CreateAccount(1000, "acc1");
  banking_system_->CreateAccount(1001, "acc2");

  banking_system_->Deposit(1002, "acc1", 1000);

  // Valid transfer
  auto result = banking_system_->Transfer(1003, "acc1", "acc2", 300);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 700);  // acc1 balance after transfer

  // Check acc2 balance
  auto balance = banking_system_->GetBalance(1004, "acc2", 1004);
  ASSERT_TRUE(balance.has_value());
  EXPECT_EQ(*balance, 300);

  // Insufficient funds
  result = banking_system_->Transfer(1005, "acc1", "acc2", 800);
  EXPECT_FALSE(result.has_value());

  // Transfer from/to non-existent account
  result = banking_system_->Transfer(1006, "nonexistent", "acc2", 100);
  EXPECT_FALSE(result.has_value());
}

TEST_F(BankingSystemTest, TopSpenders) {
  banking_system_->CreateAccount(1000, "acc1");
  banking_system_->CreateAccount(1001, "acc2");
  banking_system_->CreateAccount(1002, "acc3");

  banking_system_->Deposit(1003, "acc1", 1000);
  banking_system_->Deposit(1004, "acc2", 1000);

  // Make some transfers to create spending history
  banking_system_->Transfer(1005, "acc1", "acc2", 100);
  banking_system_->Transfer(1006, "acc1", "acc3", 200);
  banking_system_->Transfer(1007, "acc2", "acc3", 50);

  auto spenders = banking_system_->TopSpenders(1008, 2);
  ASSERT_EQ(spenders.size(), 2);
  EXPECT_EQ(spenders[0], "acc1(300)");  // acc1 spent 300 total
  EXPECT_EQ(spenders[1], "acc2(50)");   // acc2 spent 50 total
}

// Lock-free queue tests
TEST(LockFreeQueueTest, BasicOperations) {
  concurrent::LockFreeQueue<int> queue;

  // Test enqueue and dequeue
  queue.enqueue(42);
  queue.enqueue(24);

  auto result1 = queue.dequeue();
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(*result1, 42);

  auto result2 = queue.dequeue();
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(*result2, 24);

  // Queue should be empty
  auto result3 = queue.dequeue();
  EXPECT_FALSE(result3.has_value());
}

TEST(LockFreeQueueTest, ConcurrentAccess) {
  concurrent::LockFreeQueue<int> queue;
  std::atomic<int> produced{0};
  std::atomic<int> consumed{0};

  const int num_producers = 4;
  const int num_consumers = 4;
  const int items_per_producer = 1000;

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  // Start producers
  for (int p = 0; p < num_producers; ++p) {
    producers.emplace_back([&queue, &produced, items_per_producer, p]() {
      for (int i = 0; i < items_per_producer; ++i) {
        queue.enqueue(p * items_per_producer + i);
        produced.fetch_add(1);
      }
    });
  }

  // Start consumers
  for (int c = 0; c < num_consumers; ++c) {
    consumers.emplace_back([&queue, &consumed]() {
      while (consumed.load() < num_producers * items_per_producer) {
        if (auto item = queue.dequeue()) {
          consumed.fetch_add(1);
        } else {
          std::this_thread::yield();  // Wait for items
        }
      }
    });
  }

  // Wait for completion
  for (auto& t : producers) t.join();
  for (auto& t : consumers) t.join();

  EXPECT_EQ(produced.load(), num_producers * items_per_producer);
  EXPECT_EQ(consumed.load(), num_producers * items_per_producer);
  EXPECT_TRUE(queue.empty());
}

// Fraud detection agent tests
TEST(FraudDetectionAgentTest, BasicAnalysis) {
  ai::FraudDetectionAgent agent;

  ai::TransactionData tx("acc1", "TRANSFER", 1000, 1000);
  auto result = agent.analyzeTransaction(tx);

  // Should have low risk for first transaction
  EXPECT_LT(result.risk_score, 0.5);
  EXPECT_EQ(result.recommendation, "ALLOW");
}

TEST(FraudDetectionAgentTest, AnomalyDetection) {
  ai::FraudDetectionAgent agent;

  // Add some normal transactions
  for (int i = 0; i < 10; ++i) {
    ai::TransactionData tx("acc1", "TRANSFER", 100, 1000 + i * 60);
    agent.analyzeTransaction(tx);
  }

  // Add anomalous transaction
  ai::TransactionData anomalous_tx("acc1", "TRANSFER", 10000, 2000);
  auto result = agent.analyzeTransaction(anomalous_tx);

  // Should detect anomaly
  EXPECT_GT(result.risk_score, 0.5);
  EXPECT_TRUE(result.risk_factors.size() > 0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
