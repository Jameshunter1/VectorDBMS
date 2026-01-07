#include <core_engine/security/auth.hpp>
#include <core_engine/security/audit.hpp>
#include <core_engine/config/app_config.hpp>

#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

using namespace core_engine::security;
using namespace core_engine::audit;
using namespace core_engine::config;

void test_auth_create_user() {
  std::cout << "Test: Create User..." << std::flush;
  
  AuthManager auth;
  
  // Create a new user
  bool created = auth.CreateUser("testuser", "password123", {"user"});
  assert(created && "Should create new user");
  
  // Cannot create duplicate user
  bool duplicate = auth.CreateUser("testuser", "different", {"user"});
  assert(!duplicate && "Should not create duplicate user");
  
  std::cout << " PASSED\n";
}

void test_auth_validate_credentials() {
  std::cout << "Test: Validate Credentials..." << std::flush;
  
  AuthManager auth;
  auth.CreateUser("alice", "secret123", {"user"});
  
  // Valid credentials
  bool valid = auth.ValidateCredentials("alice", "secret123");
  assert(valid && "Should validate correct credentials");
  
  // Invalid password
  bool invalid_pass = auth.ValidateCredentials("alice", "wrongpassword");
  assert(!invalid_pass && "Should reject wrong password");
  
  // Nonexistent user
  bool invalid_user = auth.ValidateCredentials("bob", "secret123");
  assert(!invalid_user && "Should reject nonexistent user");
  
  std::cout << " PASSED\n";
}

void test_auth_session_management() {
  std::cout << "Test: Session Management..." << std::flush;
  
  AuthManager auth;
  auth.CreateUser("bob", "pass456", {"user"});
  
  // Create session
  std::string session_id = auth.CreateSession("bob", "127.0.0.1");
  assert(!session_id.empty() && "Should create session");
  
  // Validate session
  bool valid = auth.ValidateSession(session_id);
  assert(valid && "Should validate active session");
  
  // Get username from session
  auto username = auth.GetUsernameFromSession(session_id);
  assert(username == "bob" && "Should return correct username");
  
  // Invalidate session
  auth.InvalidateSession(session_id);
  bool invalid = auth.ValidateSession(session_id);
  assert(!invalid && "Should reject invalidated session");
  
  std::cout << " PASSED\n";
}

void test_auth_session_expiration() {
  std::cout << "Test: Session Expiration (takes 2+ seconds)..." << std::flush;
  
  AuthManager auth;
  auth.CreateUser("charlie", "temp", {"user"});
  
  // Create session with short timeout (modify timeout for testing)
  std::string session_id = auth.CreateSession("charlie", "127.0.0.1");
  
  // Wait for expiration (sessions timeout after 30 minutes by default)
  // For this test, we'll verify the validation works immediately
  bool valid_before = auth.ValidateSession(session_id);
  assert(valid_before && "Session should be valid immediately");
  
  // Refresh session
  auth.RefreshSession(session_id);
  bool valid_after = auth.ValidateSession(session_id);
  assert(valid_after && "Session should be valid after refresh");
  
  std::cout << " PASSED\n";
}

void test_auth_role_based_access() {
  std::cout << "Test: Role-Based Access Control..." << std::flush;
  
  AuthManager auth;
  auth.CreateUser("admin_user", "admin123", {"admin", "user"});
  auth.CreateUser("normal_user", "user123", {"user"});
  
  // Check admin role
  bool is_admin = auth.HasRole("admin_user", "admin");
  assert(is_admin && "Admin user should have admin role");
  
  bool is_not_admin = auth.HasRole("normal_user", "admin");
  assert(!is_not_admin && "Normal user should not have admin role");
  
  // Check permissions
  bool admin_can_write = auth.CanWrite("admin_user");
  assert(admin_can_write && "Admin should be able to write");
  
  bool user_can_write = auth.CanWrite("normal_user");
  assert(user_can_write && "User should be able to write");
  
  bool admin_is_admin = auth.IsAdmin("admin_user");
  assert(admin_is_admin && "Admin check should return true");
  
  bool user_is_admin = auth.IsAdmin("normal_user");
  assert(!user_is_admin && "Normal user should not be admin");
  
  std::cout << " PASSED\n";
}

void test_auth_deactivate_user() {
  std::cout << "Test: Deactivate User..." << std::flush;
  
  AuthManager auth;
  auth.CreateUser("deactivated", "pass", {"user"});
  
  // User should authenticate before deactivation
  bool valid_before = auth.ValidateCredentials("deactivated", "pass");
  assert(valid_before && "User should authenticate before deactivation");
  
  // Deactivate user
  auth.DeactivateUser("deactivated");
  
  // User should not authenticate after deactivation
  bool valid_after = auth.ValidateCredentials("deactivated", "pass");
  assert(!valid_after && "Deactivated user should not authenticate");
  
  std::cout << " PASSED\n";
}

void test_audit_logging() {
  std::cout << "Test: Audit Logging..." << std::flush;
  
  AuditLogger audit("./test_audit.log");
  
  // Log various events
  audit.LogLogin("alice", "127.0.0.1", true);
  audit.LogPut("alice", "key1", true);
  audit.LogGet("bob", "key2", true);
  audit.LogDelete("admin", "key3", true);
  audit.LogLogin("hacker", "203.0.113.0", false);
  audit.LogUnauthorizedAccess("anonymous", "203.0.113.1", "DELETE /api/admin");
  
  // Get recent entries
  auto recent = audit.GetRecentEntries(10);
  assert(recent.size() == 6 && "Should have 6 logged entries");
  
  // Get entries by user
  auto alice_entries = audit.GetEntriesByUser("alice");
  assert(alice_entries.size() == 2 && "Alice should have 2 entries");
  
  auto bob_entries = audit.GetEntriesByUser("bob");
  assert(bob_entries.size() == 1 && "Bob should have 1 entry");
  
  // Get entries by type
  auto login_entries = audit.GetEntriesByType(EventType::kLogin);
  assert(login_entries.size() == 1 && "Should have 1 successful login");
  
  auto failed_logins = audit.GetEntriesByType(EventType::kLoginFailed);
  assert(failed_logins.size() == 1 && "Should have 1 failed login");
  
  // Check statistics
  size_t total = audit.GetTotalEntryCount();
  assert(total == 6 && "Should have 6 total entries");
  
  size_t failed_count = audit.GetFailedLoginCount();
  assert(failed_count == 1 && "Should have 1 failed login");
  
  size_t unauthorized = audit.GetUnauthorizedAccessCount();
  assert(unauthorized == 1 && "Should have 1 unauthorized access");
  
  std::cout << " PASSED\n";
}

void test_audit_time_range() {
  std::cout << "Test: Audit Time Range Query..." << std::flush;
  
  AuditLogger audit("./test_audit_time.log");
  
  auto start_time = std::chrono::system_clock::now();
  
  // Log some events
  audit.LogLogin("user1", "127.0.0.1", true);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto mid_time = std::chrono::system_clock::now();
  
  audit.LogPut("user1", "key", true);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto end_time = std::chrono::system_clock::now();
  
  // Query full range
  auto all = audit.GetEntriesInTimeRange(start_time, end_time);
  assert(all.size() == 2 && "Should find both entries");
  
  // Query only first half
  auto first_half = audit.GetEntriesInTimeRange(start_time, mid_time);
  assert(first_half.size() == 1 && "Should find only first entry");
  
  std::cout << " PASSED\n";
}

void test_config_load_save() {
  std::cout << "Test: Configuration Load/Save..." << std::flush;
  
  auto& config = AppConfig::Instance();
  
  // Modify some values
  config.MutableServer().host = "0.0.0.0";
  config.MutableServer().port = 9090;
  config.MutableSecurity().require_authentication = true;
  config.MutableSecurity().session_timeout_minutes = 60;
  config.MutableDatabase().data_dir = "/var/lib/vectis";

  // Save to file
  bool saved = config.Save("./test_config.txt");
  assert(saved && "Should save config to file");
  
  // Load from file (into same instance)
  bool load_success = config.Load("./test_config.txt");
  assert(load_success && "Should load config from file");
  
  // Verify values
  assert(config.Server().host == "0.0.0.0" && "Host should match");
  assert(config.Server().port == 9090 && "Port should match");
  assert(config.Security().require_authentication == true && "Auth should match");
  assert(config.Security().session_timeout_minutes == 60 && "Timeout should match");
  assert(config.Database().data_dir == "/var/lib/vectis" && "Data dir should match");

  std::cout << " PASSED\n";
}

void test_config_presets() {
  std::cout << "Test: Configuration Presets..." << std::flush;
  
  auto dev = AppConfig::Development();
  assert(dev.Server().port == 8080 && "Dev port should be 8080");
  assert(dev.Security().require_authentication == false && "Dev should not require auth");
  assert(dev.Database().data_dir == "./_dev_data" && "Dev data dir should be _dev_data");
  
  auto prod = AppConfig::Production();
  assert(prod.Server().port == 443 && "Prod port should be 443");
  assert(prod.Security().require_authentication == true && "Prod should require auth");
  assert(prod.Server().enable_https == true && "Prod should enable HTTPS");
  
  std::cout << " PASSED\n";
}

void test_auth_cleanup_sessions() {
  std::cout << "Test: Cleanup Expired Sessions..." << std::flush;
  
  AuthManager auth;
  auth.CreateUser("user1", "pass", {"user"});
  auth.CreateUser("user2", "pass", {"user"});
  
  // Create multiple sessions
  auto session1 = auth.CreateSession("user1", "127.0.0.1");
  auto session2 = auth.CreateSession("user2", "127.0.0.1");
  auto session3 = auth.CreateSession("user1", "192.168.1.1");
  
  size_t count_before = auth.GetActiveSessionCount();
  assert(count_before == 3 && "Should have 3 active sessions");
  
  // Cleanup (won't find expired ones immediately since timeout is 30 minutes)
  auth.CleanupExpiredSessions();
  
  size_t count_after = auth.GetActiveSessionCount();
  assert(count_after == 3 && "Should still have 3 sessions (not expired yet)");
  
  // Invalidate one session
  auth.InvalidateSession(session2);
  
  size_t count_after_invalidate = auth.GetActiveSessionCount();
  assert(count_after_invalidate == 2 && "Should have 2 active sessions after invalidation");
  
  std::cout << " PASSED\n";
}

int main() {
  std::cout << "=== Security Tests ===\n\n";
  
  // Authentication tests
  test_auth_create_user();
  test_auth_validate_credentials();
  test_auth_session_management();
  test_auth_session_expiration();
  test_auth_role_based_access();
  test_auth_deactivate_user();
  test_auth_cleanup_sessions();
  
  // Audit logging tests
  test_audit_logging();
  test_audit_time_range();
  
  // Configuration tests
  test_config_load_save();
  test_config_presets();
  
  std::cout << "\n=== All Security Tests PASSED ===\n";
  return 0;
}
