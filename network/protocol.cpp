#include "protocol.hpp"

#include <iomanip>
#include <sstream>

namespace banking {
namespace network {
namespace protocol {

// Request helper methods
Request Request::createAccount(int timestamp, const std::string& client_id,
                              const std::string& session_token,
                              const std::string& account_id) {
  Request req;
  req.type = MessageType::CREATE_ACCOUNT;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = session_token;
  req.payload["account_id"] = account_id;
  return req;
}

Request Request::deposit(int timestamp, const std::string& client_id,
                        const std::string& session_token,
                        const std::string& account_id, int amount) {
  Request req;
  req.type = MessageType::DEPOSIT;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = session_token;
  req.payload["account_id"] = account_id;
  req.payload["amount"] = amount;
  return req;
}

Request Request::transfer(int timestamp, const std::string& client_id,
                         const std::string& session_token,
                         const std::string& source_account,
                         const std::string& target_account, int amount) {
  Request req;
  req.type = MessageType::TRANSFER;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = session_token;
  req.payload["source_account"] = source_account;
  req.payload["target_account"] = target_account;
  req.payload["amount"] = amount;
  return req;
}

Request Request::getBalance(int timestamp, const std::string& client_id,
                           const std::string& session_token,
                           const std::string& account_id, int time_at) {
  Request req;
  req.type = MessageType::GET_BALANCE;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = session_token;
  req.payload["account_id"] = account_id;
  req.payload["time_at"] = time_at;
  return req;
}

Request Request::topSpenders(int timestamp, const std::string& client_id,
                            const std::string& session_token, int n) {
  Request req;
  req.type = MessageType::TOP_SPENDERS;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = session_token;
  req.payload["n"] = n;
  return req;
}

Request Request::schedulePayment(int timestamp, const std::string& client_id,
                                const std::string& session_token,
                                const std::string& account_id, int amount, int delay) {
  Request req;
  req.type = MessageType::SCHEDULE_PAYMENT;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = session_token;
  req.payload["account_id"] = account_id;
  req.payload["amount"] = amount;
  req.payload["delay"] = delay;
  return req;
}

Request Request::cancelPayment(int timestamp, const std::string& client_id,
                              const std::string& session_token,
                              const std::string& account_id, const std::string& payment_id) {
  Request req;
  req.type = MessageType::CANCEL_PAYMENT;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = session_token;
  req.payload["account_id"] = account_id;
  req.payload["payment_id"] = payment_id;
  return req;
}

Request Request::mergeAccounts(int timestamp, const std::string& client_id,
                              const std::string& session_token,
                              const std::string& account_id_1,
                              const std::string& account_id_2) {
  Request req;
  req.type = MessageType::MERGE_ACCOUNTS;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = session_token;
  req.payload["account_id_1"] = account_id_1;
  req.payload["account_id_2"] = account_id_2;
  return req;
}

Request Request::authenticate(int timestamp, const std::string& username,
                             const std::string& password) {
  Request req;
  req.type = MessageType::AUTHENTICATE;
  req.timestamp = timestamp;
  req.client_id = "";
  req.session_token = "";
  req.payload["username"] = username;
  req.payload["password"] = password;
  return req;
}

Request Request::heartbeat(int timestamp, const std::string& client_id) {
  Request req;
  req.type = MessageType::HEARTBEAT;
  req.timestamp = timestamp;
  req.client_id = client_id;
  req.session_token = "";
  req.payload = nlohmann::json::object();
  return req;
}

// Response helper methods
Response Response::success(const std::string& message, int timestamp,
                          const nlohmann::json& payload) {
  Response resp;
  resp.status = Status::SUCCESS;
  resp.message = message;
  resp.timestamp = timestamp;
  resp.payload = payload;
  return resp;
}

Response Response::error(Status status, const std::string& message, int timestamp) {
  Response resp;
  resp.status = status;
  resp.message = message;
  resp.timestamp = timestamp;
  resp.payload = nlohmann::json::object();
  return resp;
}

Response Response::accountCreated(const std::string& account_id, int timestamp) {
  nlohmann::json payload;
  payload["account_id"] = account_id;
  return success("Account created successfully", timestamp, payload);
}

Response Response::depositResult(int new_balance, int timestamp) {
  nlohmann::json payload;
  payload["balance"] = new_balance;
  return success("Deposit successful", timestamp, payload);
}

Response Response::transferResult(int new_source_balance, int timestamp) {
  nlohmann::json payload;
  payload["source_balance"] = new_source_balance;
  return success("Transfer successful", timestamp, payload);
}

Response Response::balanceResult(int balance, int timestamp) {
  nlohmann::json payload;
  payload["balance"] = balance;
  return success("Balance retrieved", timestamp, payload);
}

Response Response::topSpendersResult(const std::vector<std::string>& spenders, int timestamp) {
  nlohmann::json payload;
  payload["spenders"] = spenders;
  return success("Top spenders retrieved", timestamp, payload);
}

Response Response::paymentScheduled(const std::string& payment_id, int timestamp) {
  nlohmann::json payload;
  payload["payment_id"] = payment_id;
  return success("Payment scheduled", timestamp, payload);
}

Response Response::paymentCancelled(int timestamp) {
  return success("Payment cancelled", timestamp);
}

Response Response::accountsMerged(int timestamp) {
  return success("Accounts merged", timestamp);
}

Response Response::authenticated(const std::string& session_token, int timestamp) {
  nlohmann::json payload;
  payload["session_token"] = session_token;
  return success("Authentication successful", timestamp, payload);
}

// Serialization functions
std::string serializeRequest(const Request& request) {
  nlohmann::json j;
  j["type"] = static_cast<int>(request.type);
  j["timestamp"] = request.timestamp;
  j["client_id"] = request.client_id;
  j["session_token"] = request.session_token;
  j["payload"] = request.payload;
  return j.dump();
}

Request deserializeRequest(const std::string& json_str) {
  nlohmann::json j = nlohmann::json::parse(json_str);
  Request req;
  req.type = static_cast<MessageType>(j["type"].get<int>());
  req.timestamp = j["timestamp"];
  req.client_id = j["client_id"];
  req.session_token = j["session_token"];
  req.payload = j["payload"];
  return req;
}

std::string serializeResponse(const Response& response) {
  nlohmann::json j;
  j["status"] = static_cast<int>(response.status);
  j["message"] = response.message;
  j["timestamp"] = response.timestamp;
  j["payload"] = response.payload;
  return j.dump();
}

Response deserializeResponse(const std::string& json_str) {
  nlohmann::json j = nlohmann::json::parse(json_str);
  Response resp;
  resp.status = static_cast<Status>(j["status"].get<int>());
  resp.message = j["message"];
  resp.timestamp = j["timestamp"];
  resp.payload = j["payload"];
  return resp;
}

// Message framing implementation
std::string MessageFramer::frameMessage(const std::string& message) {
  std::stringstream ss;
  ss << std::setw(8) << std::setfill('0') << std::hex << message.size();
  ss << message;
  return ss.str();
}

std::string MessageFramer::unframeMessage(const std::string& framed_message) {
  if (framed_message.size() < 8) {
    throw std::runtime_error("Invalid framed message: too short");
  }

  std::stringstream ss(framed_message.substr(0, 8));
  size_t message_size;
  ss >> std::hex >> message_size;

  if (framed_message.size() < 8 + message_size) {
    throw std::runtime_error("Invalid framed message: incomplete");
  }

  return framed_message.substr(8, message_size);
}

bool MessageFramer::isCompleteMessage(const std::string& buffer) {
  if (buffer.size() < 8) return false;

  std::stringstream ss(buffer.substr(0, 8));
  size_t message_size;
  ss >> std::hex >> message_size;

  return buffer.size() >= 8 + message_size;
}

}  // namespace protocol
}  // namespace network
}  // namespace banking
