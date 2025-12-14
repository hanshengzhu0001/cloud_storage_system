#include "postgres_connection.hpp"

#include <sstream>
#include <iostream>
#include <cstring>

namespace banking {
namespace database {

PostgresConnection::PostgresConnection(const Config& config)
    : config_(config), connection_(nullptr), in_transaction_(false) {
}

PostgresConnection::~PostgresConnection() {
  disconnect();
}

bool PostgresConnection::connect() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (connection_) {
    disconnect();
  }

  // Build connection string
  std::stringstream conn_str;
  conn_str << "host=" << config_.host
           << " port=" << config_.port
           << " dbname=" << config_.database
           << " user=" << config_.username
           << " password=" << config_.password
           << " connect_timeout=" << config_.connection_timeout;

  connection_ = PQconnectdb(conn_str.str().c_str());

  if (PQstatus(connection_) != CONNECTION_OK) {
    std::cerr << "Database connection failed: " << PQerrorMessage(connection_) << std::endl;
    disconnect();
    return false;
  }

  // Set session parameters for better performance
  executeQuery("SET SESSION synchronous_commit = off;");
  executeQuery("SET SESSION work_mem = '64MB';");
  executeQuery("SET SESSION maintenance_work_mem = '256MB';");

  std::cout << "Connected to PostgreSQL database: " << getConnectionInfo() << std::endl;
  return true;
}

void PostgresConnection::disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (connection_) {
    if (in_transaction_) {
      rollbackTransaction();
    }
    PQfinish(connection_);
    connection_ = nullptr;
  }
}

bool PostgresConnection::isConnected() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return connection_ && PQstatus(connection_) == CONNECTION_OK;
}

bool PostgresConnection::executeQuery(const std::string& query) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!connection_) return false;

  PGresult* result = PQexec(connection_, query.c_str());

  if (!result) {
    std::cerr << "Query execution failed: connection lost" << std::endl;
    return false;
  }

  ExecStatusType status = PQresultStatus(result);
  bool success = (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK);

  if (!success) {
    std::cerr << "Query failed: " << PQresultErrorMessage(result) << std::endl;
  }

  PQclear(result);
  return success;
}

PGresult* PostgresConnection::executeQueryWithResult(const std::string& query) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!connection_) return nullptr;

  PGresult* result = PQexec(connection_, query.c_str());

  if (!result) {
    std::cerr << "Query execution failed: connection lost" << std::endl;
    return nullptr;
  }

  ExecStatusType status = PQresultStatus(result);
  if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
    std::cerr << "Query failed: " << PQresultErrorMessage(result) << std::endl;
    PQclear(result);
    return nullptr;
  }

  return result;
}

PGresult* PostgresConnection::executeParameterizedQuery(const std::string& query,
                                                       int nParams,
                                                       const char* const* paramValues,
                                                       const int* paramLengths,
                                                       const int* paramFormats) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!connection_) return nullptr;

  PGresult* result = PQexecParams(connection_, query.c_str(), nParams, nullptr,
                                 paramValues, paramLengths, paramFormats, 0);

  if (!result) {
    std::cerr << "Parameterized query execution failed: connection lost" << std::endl;
    return nullptr;
  }

  ExecStatusType status = PQresultStatus(result);
  if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
    std::cerr << "Parameterized query failed: " << PQresultErrorMessage(result) << std::endl;
    PQclear(result);
    return nullptr;
  }

  return result;
}

bool PostgresConnection::beginTransaction() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!executeQuery("BEGIN")) {
    return false;
  }

  in_transaction_ = true;
  return true;
}

bool PostgresConnection::commitTransaction() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!in_transaction_) {
    return false;
  }

  bool success = executeQuery("COMMIT");
  in_transaction_ = false;
  return success;
}

bool PostgresConnection::rollbackTransaction() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!in_transaction_) {
    return false;
  }

  bool success = executeQuery("ROLLBACK");
  in_transaction_ = false;
  return success;
}

std::string PostgresConnection::getLastError() const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!connection_) {
    return "Not connected";
  }

  return PQerrorMessage(connection_);
}

std::string PostgresConnection::getConnectionInfo() const {
  std::stringstream ss;
  ss << config_.username << "@" << config_.host << ":" << config_.port << "/" << config_.database;
  return ss.str();
}

// TransactionGuard implementation
TransactionGuard::TransactionGuard(PostgresConnection& conn)
    : conn_(conn), committed_(false) {
  if (!conn_.beginTransaction()) {
    throw std::runtime_error("Failed to begin transaction");
  }
}

TransactionGuard::~TransactionGuard() {
  if (!committed_) {
    conn_.rollbackTransaction();
  }
}

void TransactionGuard::commit() {
  if (!committed_ && conn_.commitTransaction()) {
    committed_ = true;
  }
}

void TransactionGuard::rollback() {
  if (!committed_) {
    conn_.rollbackTransaction();
    committed_ = true;
  }
}

}  // namespace database
}  // namespace banking
