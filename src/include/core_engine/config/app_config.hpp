#pragma once

#include <map>
#include <string>

namespace core_engine {
namespace config {

// Server configuration
struct ServerConfig {
  std::string host{"127.0.0.1"};
  int port{8080};
  bool enable_https{false};
  std::string cert_path;
  std::string key_path;
  int max_connections{100};
  int timeout_seconds{30};
};

// Security configuration
struct SecurityConfig {
  bool require_authentication{true};
  int session_timeout_minutes{30};
  int max_login_attempts{5};
  int rate_limit_per_minute{60};
  bool enable_audit_log{true};
  std::string audit_log_path{"./audit.log"};
};

// Database configuration
struct DatabaseConfig {
  std::string data_dir{"./_data"};
  size_t buffer_pool_size_mb{4};
  size_t wal_buffer_size_kb{256};
  bool enable_compression{false};
  int compaction_threads{2};
};

// Application configuration
class AppConfig {
 public:
  static AppConfig& Instance();
  
  // Load configuration from file
  bool Load(const std::string& config_file);
  
  // Save configuration to file
  bool Save(const std::string& config_file) const;
  
  // Accessors
  const ServerConfig& Server() const { return server_; }
  const SecurityConfig& Security() const { return security_; }
  const DatabaseConfig& Database() const { return database_; }
  
  // Mutators
  ServerConfig& MutableServer() { return server_; }
  SecurityConfig& MutableSecurity() { return security_; }
  DatabaseConfig& MutableDatabase() { return database_; }
  
  // Default configurations
  static AppConfig Development();
  static AppConfig Production();
  
 private:
  AppConfig() = default;
  
  ServerConfig server_;
  SecurityConfig security_;
  DatabaseConfig database_;
};

}  // namespace config
}  // namespace core_engine
