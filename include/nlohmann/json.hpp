// Simplified JSON header for demonstration purposes
// In a real implementation, use the actual nlohmann/json library

#ifndef NLOHMANN_JSON_HPP_
#define NLOHMANN_JSON_HPP_

#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>

namespace nlohmann {

class json {
 public:
  // Types
  enum class value_t { null, object, array, string, boolean, number_integer, number_unsigned, number_float };

  // Constructors
  json() : type_(value_t::null) {}
  json(const std::string& str) : type_(value_t::string), string_value_(str) {}
  json(int val) : type_(value_t::number_integer), int_value_(val) {}
  json(double val) : type_(value_t::number_float), float_value_(val) {}
  json(bool val) : type_(value_t::boolean), bool_value_(val) {}

  // Object operations
  json& operator[](const std::string& key) {
    if (type_ != value_t::object) {
      *this = json::object();
    }
    return object_value_[key];
  }

  const json& operator[](const std::string& key) const {
    static const json null_json;
    if (type_ != value_t::object) return null_json;
    auto it = object_value_.find(key);
    return it != object_value_.end() ? it->second : null_json;
  }

  // Array operations
  json& operator[](size_t index) {
    if (type_ != value_t::array) {
      *this = json::array();
    }
    if (index >= array_value_.size()) {
      array_value_.resize(index + 1);
    }
    return array_value_[index];
  }

  // Assignment
  json& operator=(const std::string& str) {
    type_ = value_t::string;
    string_value_ = str;
    return *this;
  }

  json& operator=(int val) {
    type_ = value_t::number_integer;
    int_value_ = val;
    return *this;
  }

  json& operator=(double val) {
    type_ = value_t::number_float;
    float_value_ = val;
    return *this;
  }

  json& operator=(bool val) {
    type_ = value_t::boolean;
    bool_value_ = val;
    return *this;
  }

  // Contains
  bool contains(const std::string& key) const {
    return type_ == value_t::object && object_value_.find(key) != object_value_.end();
  }

  // Getters
  std::string get_string() const {
    if (type_ == value_t::string) return string_value_;
    throw std::runtime_error("Not a string");
  }

  int get_int() const {
    if (type_ == value_t::number_integer) return int_value_;
    throw std::runtime_error("Not an integer");
  }

  double get_float() const {
    if (type_ == value_t::number_float) return float_value_;
    throw std::runtime_error("Not a float");
  }

  bool get_bool() const {
    if (type_ == value_t::boolean) return bool_value_;
    throw std::runtime_error("Not a boolean");
  }

  // Serialization
  std::string dump(int indent = -1) const {
    std::stringstream ss;
    dump_internal(ss, indent, 0);
    return ss.str();
  }

  // Parsing
  static json parse(const std::string& str) {
    // Very basic JSON parser for demonstration
    json result;
    if (str.empty()) return result;

    if (str[0] == '{') {
      result.type_ = value_t::object;
      // Simplified parsing - in real implementation, use proper JSON parser
      result.object_value_["parsed"] = json(true);
    } else if (str[0] == '[') {
      result.type_ = value_t::array;
    } else if (str[0] == '"') {
      result.type_ = value_t::string;
      result.string_value_ = str.substr(1, str.size() - 2);
    } else if (str == "true") {
      result.type_ = value_t::boolean;
      result.bool_value_ = true;
    } else if (str == "false") {
      result.type_ = value_t::boolean;
      result.bool_value_ = false;
    } else if (str.find('.') != std::string::npos) {
      result.type_ = value_t::number_float;
      result.float_value_ = std::stod(str);
    } else {
      result.type_ = value_t::number_integer;
      result.int_value_ = std::stoi(str);
    }

    return result;
  }

  // Static factory methods
  static json object() {
    json j;
    j.type_ = value_t::object;
    return j;
  }

  static json array() {
    json j;
    j.type_ = value_t::array;
    return j;
  }

 private:
  value_t type_;
  std::string string_value_;
  int int_value_{0};
  double float_value_{0.0};
  bool bool_value_{false};
  std::map<std::string, json> object_value_;
  std::vector<json> array_value_;

  void dump_internal(std::stringstream& ss, int indent, int level) const {
    std::string indent_str(indent > 0 ? std::string(indent * level, ' ') : "");

    switch (type_) {
      case value_t::null:
        ss << "null";
        break;
      case value_t::string:
        ss << "\"" << string_value_ << "\"";
        break;
      case value_t::number_integer:
        ss << int_value_;
        break;
      case value_t::number_unsigned:
        ss << int_value_;  // Use int_value_ for unsigned as well
        break;
      case value_t::number_float:
        ss << float_value_;
        break;
      case value_t::boolean:
        ss << (bool_value_ ? "true" : "false");
        break;
      case value_t::object:
        ss << "{";
        if (!object_value_.empty()) {
          ss << "\n";
          bool first = true;
          for (const auto& [key, value] : object_value_) {
            if (!first) ss << ",\n";
            ss << indent_str << std::string(indent > 0 ? indent : 2, ' ')
               << "\"" << key << "\": ";
            value.dump_internal(ss, indent, level + 1);
            first = false;
          }
          ss << "\n" << indent_str;
        }
        ss << "}";
        break;
      case value_t::array:
        ss << "[";
        if (!array_value_.empty()) {
          ss << "\n";
          bool first = true;
          for (const auto& value : array_value_) {
            if (!first) ss << ",\n";
            ss << indent_str << std::string(indent > 0 ? indent : 2, ' ');
            value.dump_internal(ss, indent, level + 1);
            first = false;
          }
          ss << "\n" << indent_str;
        }
        ss << "]";
        break;
    }
  }
};

}  // namespace nlohmann

#endif  // NLOHMANN_JSON_HPP_
