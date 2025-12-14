#include "include/banking_server.hpp"
#include "include/banking_system_persistent.hpp"

#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signalHandler(int signum) {
  std::cout << "\nReceived signal " << signum << ". Shutting down..." << std::endl;
  running = false;
}

int main(int argc, char* argv[]) {
  int port = 8080;
  size_t num_workers = 4;
  size_t analysis_window = 3600;  // 1 hour

  // Database configuration (optional)
  std::string db_host = "localhost";
  int db_port = 5432;
  std::string db_name = "banking_system";
  std::string db_username = "banking_user";
  std::string db_password = "";

  // Parse command line arguments
  if (argc >= 2) port = std::stoi(argv[1]);
  if (argc >= 3) num_workers = std::stoul(argv[2]);
  if (argc >= 4) analysis_window = std::stoul(argv[3]);
  if (argc >= 5) db_host = argv[4];
  if (argc >= 6) db_port = std::stoi(argv[5]);
  if (argc >= 7) db_name = argv[6];
  if (argc >= 8) db_username = argv[7];
  if (argc >= 9) db_password = argv[8];

  std::cout << "=== Distributed Banking System Server ===" << std::endl;
  std::cout << "Port: " << port << std::endl;
  std::cout << "Worker threads: " << num_workers << std::endl;
  std::cout << "Fraud analysis window: " << analysis_window << " seconds" << std::endl;
  std::cout << "Database: " << db_username << "@" << db_host << ":" << db_port << "/" << db_name << std::endl;
  std::cout << "==========================================" << std::endl;

  // Set up signal handling
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  try {
    // Determine if we should use persistent storage
    bool use_database = !db_host.empty() && !db_username.empty();

    std::unique_ptr<banking::BankingServer> server;

    if (use_database) {
      // Use persistent banking system with PostgreSQL
      banking::BankingSystemPersistent::Config db_config{
        db_host, db_port, db_name, db_username, db_password
      };

      std::cout << "Initializing with PostgreSQL persistence..." << std::endl;
      auto persistent_system = std::make_unique<banking::BankingSystemPersistent>(db_config);

      if (!persistent_system->initialize()) {
        std::cerr << "Failed to initialize persistent banking system" << std::endl;
        return 1;
      }

      // Wrap persistent system in thread-safe wrapper
      auto thread_safe_system = std::make_unique<banking::BankingSystemThreadSafe>(std::move(persistent_system));
      server = std::make_unique<banking::BankingServer>(port, num_workers, analysis_window,
                                                       std::move(thread_safe_system));
    } else {
      // Use in-memory system only
      std::cout << "Initializing with in-memory storage only..." << std::endl;
      server = std::make_unique<banking::BankingServer>(port, num_workers, analysis_window);
    }

    if (!server->start()) {
      std::cerr << "Failed to start banking server" << std::endl;
      return 1;
    }

    std::cout << "\nServer started successfully!" << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    // Main server loop
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(5));

      // Print statistics every 5 seconds
      auto stats = server.getStats();
      std::cout << "\n--- Server Statistics ---" << std::endl;
      std::cout << "Active connections: " << stats.active_connections << std::endl;
      std::cout << "Transactions processed: " << stats.transaction_stats.transactions_processed << std::endl;
      std::cout << "Transactions in queue: " << stats.transaction_stats.transactions_queued << std::endl;
      std::cout << "Avg processing time: " << stats.transaction_stats.avg_processing_time_ms << " ms" << std::endl;
      std::cout << "Fraud alerts generated: " << stats.fraud_stats.fraud_alerts_generated << std::endl;
      std::cout << "Avg fraud risk score: " << stats.fraud_stats.average_risk_score << std::endl;
      std::cout << "-----------------------" << std::endl;
    }

    server.stop();

  } catch (const std::exception& e) {
    std::cerr << "Server error: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "Server shutdown complete." << std::endl;
  return 0;
}
