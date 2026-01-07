#include <core_engine/config/app_config.hpp>

#include <fstream>
#include <sstream>

namespace core_engine {
namespace config {

AppConfig& AppConfig::Instance() {
  static AppConfig instance;
  return instance;
}

bool AppConfig::Load(const std::string& config_file) {
  std::ifstream file(config_file);
  if (!file.is_open()) {
    return false;
  }

  // Simple key=value parser
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue; // Skip comments and empty lines
    }

    auto pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);

    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);

    // Parse configuration values
    if (key == "server.host") {
      server_.host = value;
    } else if (key == "server.port") {
      server_.port = std::stoi(value);
    } else if (key == "server.enable_https") {
      server_.enable_https = (value == "true" || value == "1");
    } else if (key == "security.require_authentication") {
      security_.require_authentication = (value == "true" || value == "1");
    } else if (key == "security.session_timeout_minutes") {
      security_.session_timeout_minutes = std::stoi(value);
    } else if (key == "security.enable_audit_log") {
      security_.enable_audit_log = (value == "true" || value == "1");
    } else if (key == "security.audit_log_path") {
      security_.audit_log_path = value;
    } else if (key == "database.data_dir") {
      database_.data_dir = value;
    } else if (key == "database.buffer_pool_size_mb") {
      database_.buffer_pool_size_mb = std::stoull(value);
    }
  }

  return true;
}

bool AppConfig::Save(const std::string& config_file) const {
  std::ofstream file(config_file);
  if (!file.is_open()) {
    return false;
  }

  file << "# Vectis Database Engine Configuration\n\n";

  file << "# Server Settings\n";
  file << "server.host=" << server_.host << "\n";
  file << "server.port=" << server_.port << "\n";
  file << "server.enable_https=" << (server_.enable_https ? "true" : "false") << "\n\n";

  file << "# Security Settings\n";
  file << "security.require_authentication="
       << (security_.require_authentication ? "true" : "false") << "\n";
  file << "security.session_timeout_minutes=" << security_.session_timeout_minutes << "\n";
  file << "security.enable_audit_log=" << (security_.enable_audit_log ? "true" : "false") << "\n";
  file << "security.audit_log_path=" << security_.audit_log_path << "\n\n";

  file << "# Database Settings\n";
  file << "database.data_dir=" << database_.data_dir << "\n";
  file << "database.buffer_pool_size_mb=" << database_.buffer_pool_size_mb << "\n";

  return true;
}

AppConfig AppConfig::Development() {
  AppConfig config;
  config.server_.host = "127.0.0.1";
  config.server_.port = 8080;
  config.server_.enable_https = false;
  config.security_.require_authentication = false; // Disabled for dev
  config.security_.enable_audit_log = false;
  config.database_.data_dir = "./_dev_data";
  return config;
}

AppConfig AppConfig::Production() {
  AppConfig config;
  config.server_.host = "0.0.0.0";
  config.server_.port = 443;
  config.server_.enable_https = true;
  config.security_.require_authentication = true;
  config.security_.enable_audit_log = true;
  config.security_.audit_log_path = "/var/log/vectis/audit.log";
  config.database_.data_dir = "/var/lib/vectis/data";
  return config;
}

} // namespace config
} // namespace core_engine
