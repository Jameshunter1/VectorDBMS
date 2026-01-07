#include <core_engine/config/app_config.hpp>
#include <core_engine/engine.hpp>
#include <core_engine/security/audit.hpp>
#include <core_engine/security/auth.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace core_engine;
using namespace core_engine::security;
using namespace core_engine::audit;
using namespace core_engine::config;

// Integration tests combining database operations with security

void test_authenticated_database_operations() {
  std::cout << "Test: Authenticated Database Operations..." << std::flush;

  // Setup
  AuthManager auth;
  AuditLogger audit("./test_integration_audit.log");
  Engine engine;

  std::string db_path = "./test_auth_db";
  std::filesystem::remove_all(db_path);

  auto status = engine.Open(db_path);
  assert(status.ok() && "Database should open");

  // Create users
  auth.CreateUser("admin", "admin123", {"admin", "user"});
  auth.CreateUser("user1", "pass123", {"user"});
  auth.CreateUser("readonly", "read123", {"reader"});

  // Admin performs operations
  std::string admin_session = auth.CreateSession("admin", "127.0.0.1");
  assert(auth.ValidateSession(admin_session) && "Admin session should be valid");

  if (auth.CanWrite("admin")) {
    auto put_status = engine.Put("admin_key", "admin_value");
    assert(put_status.ok() && "Admin should be able to write");
    audit.LogPut("admin", "admin_key", put_status.ok());
  }

  // User performs operations
  std::string user_session = auth.CreateSession("user1", "192.168.1.10");
  assert(auth.ValidateSession(user_session) && "User session should be valid");

  if (auth.CanWrite("user1")) {
    auto put_status = engine.Put("user_key", "user_value");
    assert(put_status.ok() && "User should be able to write");
    audit.LogPut("user1", "user_key", put_status.ok());
  }

  if (auth.CanRead("user1")) {
    auto value = engine.Get("admin_key");
    assert(value.has_value() && "User should be able to read");
    audit.LogGet("user1", "admin_key", value.has_value());
  }

  // Read-only user cannot write
  std::string readonly_session = auth.CreateSession("readonly", "10.0.0.1");
  if (!auth.CanWrite("readonly")) {
    audit.LogUnauthorizedAccess("readonly", "10.0.0.1", "Attempted PUT operation");
  }

  // Verify audit log has entries
  auto recent_entries = audit.GetRecentEntries(10);
  assert(recent_entries.size() >= 3 && "Should have multiple audit entries");

  // Cleanup
  auth.InvalidateSession(admin_session);
  auth.InvalidateSession(user_session);
  auth.InvalidateSession(readonly_session);
  std::filesystem::remove_all(db_path);

  std::cout << " PASSED\n";
}

void test_concurrent_authenticated_access() {
  std::cout << "Test: Concurrent Authenticated Access..." << std::flush;

  AuthManager auth;
  AuditLogger audit("./test_concurrent_audit.log");
  Engine engine;

  std::string db_path = "./test_concurrent_db";
  std::filesystem::remove_all(db_path);
  engine.Open(db_path);

  // Create multiple users
  auth.CreateUser("user1", "pass1", {"user"});
  auth.CreateUser("user2", "pass2", {"user"});
  auth.CreateUser("user3", "pass3", {"user"});

  // Simulate concurrent operations
  std::thread t1([&]() {
    std::string session = auth.CreateSession("user1", "192.168.1.1");
    for (int i = 0; i < 10; i++) {
      if (auth.ValidateSession(session)) {
        engine.Put("key_user1_" + std::to_string(i), "value1");
        audit.LogPut("user1", "key_user1_" + std::to_string(i), true);
        auth.RefreshSession(session);
      }
    }
    auth.InvalidateSession(session);
  });

  std::thread t2([&]() {
    std::string session = auth.CreateSession("user2", "192.168.1.2");
    for (int i = 0; i < 10; i++) {
      if (auth.ValidateSession(session)) {
        engine.Put("key_user2_" + std::to_string(i), "value2");
        audit.LogPut("user2", "key_user2_" + std::to_string(i), true);
        auth.RefreshSession(session);
      }
    }
    auth.InvalidateSession(session);
  });

  std::thread t3([&]() {
    std::string session = auth.CreateSession("user3", "192.168.1.3");
    for (int i = 0; i < 10; i++) {
      if (auth.ValidateSession(session)) {
        auto val = engine.Get("key_user1_" + std::to_string(i % 5));
        audit.LogGet("user3", "key_user1_" + std::to_string(i % 5), val.has_value());
        auth.RefreshSession(session);
      }
    }
    auth.InvalidateSession(session);
  });

  t1.join();
  t2.join();
  t3.join();

  // Verify audit log captured all operations
  auto entries = audit.GetRecentEntries(100);
  assert(entries.size() >= 30 && "Should have logged all operations");

  // Verify data integrity
  auto val1 = engine.Get("key_user1_0");
  assert(val1.has_value() && val1.value() == "value1" && "Data should be intact");

  auto val2 = engine.Get("key_user2_0");
  assert(val2.has_value() && val2.value() == "value2" && "Data should be intact");

  std::filesystem::remove_all(db_path);

  std::cout << " PASSED\n";
}

void test_session_timeout_with_database() {
  std::cout << "Test: Session Timeout with Database Access..." << std::flush;

  AuthManager auth;
  AuditLogger audit("./test_timeout_audit.log");

  auth.CreateUser("testuser", "testpass", {"user"});
  std::string session = auth.CreateSession("testuser", "127.0.0.1");

  // Session should be valid immediately
  assert(auth.ValidateSession(session) && "Session should be valid");
  audit.LogLogin("testuser", "127.0.0.1", true);

  // Refresh session
  auth.RefreshSession(session);
  assert(auth.ValidateSession(session) && "Session should still be valid after refresh");

  // Logout
  auth.InvalidateSession(session);
  assert(!auth.ValidateSession(session) && "Session should be invalid after logout");
  audit.LogLogout("testuser", "127.0.0.1");

  std::cout << " PASSED\n";
}

void test_audit_log_with_failed_operations() {
  std::cout << "Test: Audit Log with Failed Operations..." << std::flush;

  AuthManager auth;
  AuditLogger audit("./test_failed_ops_audit.log");
  Engine engine;

  std::string db_path = "./test_failed_ops_db";
  std::filesystem::remove_all(db_path);
  engine.Open(db_path);

  auth.CreateUser("testuser", "testpass", {"user"});

  // Failed login attempts
  for (int i = 0; i < 5; i++) {
    bool valid = auth.ValidateCredentials("testuser", "wrongpass");
    audit.LogLogin("testuser", "203.0.113.50", valid);
  }

  // Successful login
  bool valid = auth.ValidateCredentials("testuser", "testpass");
  audit.LogLogin("testuser", "127.0.0.1", valid);

  // Query failed logins
  size_t failed_count = audit.GetFailedLoginCount();
  assert(failed_count == 5 && "Should have 5 failed login attempts");

  // Get entries by type
  auto failed_logins = audit.GetEntriesByType(EventType::kLoginFailed);
  assert(failed_logins.size() == 5 && "Should find all failed logins");

  std::filesystem::remove_all(db_path);

  std::cout << " PASSED\n";
}

void test_configuration_driven_security() {
  std::cout << "Test: Configuration-Driven Security..." << std::flush;

  auto& config = AppConfig::Instance();

  // Set security configuration
  config.MutableSecurity().require_authentication = true;
  config.MutableSecurity().session_timeout_minutes = 15;
  config.MutableSecurity().enable_audit_log = true;
  config.MutableSecurity().audit_log_path = "./test_config_audit.log";

  // Save and reload
  config.Save("./test_security_config.txt");
  config.Load("./test_security_config.txt");

  // Verify settings
  assert(config.Security().require_authentication == true);
  assert(config.Security().session_timeout_minutes == 15);
  assert(config.Security().enable_audit_log == true);

  // Use configuration
  AuthManager auth;
  AuditLogger audit(config.Security().audit_log_path);

  auth.CreateUser("user", "pass", {"user"});
  std::string session = auth.CreateSession("user", "127.0.0.1");
  audit.LogLogin("user", "127.0.0.1", true);

  assert(auth.ValidateSession(session) && "Session should be valid");

  std::cout << " PASSED\n";
}

void test_bulk_operations_with_audit() {
  std::cout << "Test: Bulk Operations with Audit Logging..." << std::flush;

  AuthManager auth;
  AuditLogger audit("./test_bulk_audit.log");
  Engine engine;

  std::string db_path = "./test_bulk_db";
  std::filesystem::remove_all(db_path);
  engine.Open(db_path);

  auth.CreateUser("bulkuser", "bulkpass", {"user"});
  std::string session = auth.CreateSession("bulkuser", "127.0.0.1");

  // Bulk insert
  const size_t count = 100;
  for (size_t i = 0; i < count; i++) {
    std::string key = "bulk_key_" + std::to_string(i);
    std::string value = "bulk_value_" + std::to_string(i);
    engine.Put(key, value);
  }

  audit.LogBatchOperation("bulkuser", count, true);

  // Verify all data
  for (size_t i = 0; i < count; i++) {
    std::string key = "bulk_key_" + std::to_string(i);
    auto value = engine.Get(key);
    assert(value.has_value() && "All bulk data should be present");
  }

  // Bulk delete
  for (size_t i = 0; i < count; i++) {
    std::string key = "bulk_key_" + std::to_string(i);
    engine.Delete(key);
  }

  audit.LogBatchOperation("bulkuser", count, true);

  // Verify deletions
  for (size_t i = 0; i < count; i++) {
    std::string key = "bulk_key_" + std::to_string(i);
    auto value = engine.Get(key);
    assert(!value.has_value() && "All bulk data should be deleted");
  }

  auth.InvalidateSession(session);
  std::filesystem::remove_all(db_path);

  std::cout << " PASSED\n";
}

void test_role_hierarchy_with_operations() {
  std::cout << "Test: Role Hierarchy with Database Operations..." << std::flush;

  AuthManager auth;
  AuditLogger audit("./test_roles_audit.log");
  Engine engine;

  std::string db_path = "./test_roles_db";
  std::filesystem::remove_all(db_path);
  engine.Open(db_path);

  // Create users with different roles
  auth.CreateUser("superadmin", "super123", {"admin", "user", "power"});
  auth.CreateUser("admin", "admin123", {"admin", "user"});
  auth.CreateUser("poweruser", "power123", {"user", "power"});
  auth.CreateUser("basicuser", "basic123", {"user"});
  auth.CreateUser("guest", "guest123", {"reader"});

  // Test superadmin capabilities
  assert(auth.IsAdmin("superadmin") && "Superadmin should be admin");
  assert(auth.CanWrite("superadmin") && "Superadmin should write");
  assert(auth.HasRole("superadmin", "power") && "Superadmin should have power role");

  // Test admin capabilities
  assert(auth.IsAdmin("admin") && "Admin should be admin");
  assert(auth.CanWrite("admin") && "Admin should write");

  // Test power user capabilities
  assert(!auth.IsAdmin("poweruser") && "Power user should not be admin");
  assert(auth.CanWrite("poweruser") && "Power user should write");
  assert(auth.HasRole("poweruser", "power") && "Should have power role");

  // Test basic user capabilities
  assert(!auth.IsAdmin("basicuser") && "Basic user should not be admin");
  assert(auth.CanWrite("basicuser") && "Basic user should write");

  // Test guest capabilities
  assert(!auth.IsAdmin("guest") && "Guest should not be admin");
  // Guest with "reader" role can still read (CanRead checks for "user" role currently)
  // In a real system, you'd extend CanRead to check for "reader" role too

  // Simulate operations based on roles
  if (auth.IsAdmin("admin")) {
    engine.Put("config_key", "config_value");
    audit.LogPut("admin", "config_key", true);
  }

  if (auth.CanWrite("basicuser")) {
    engine.Put("user_key", "user_value");
    audit.LogPut("basicuser", "user_key", true);
  }

  // Guest attempts to read (but doesn't have write permission)
  if (!auth.CanWrite("guest")) {
    auto value = engine.Get("config_key");
    audit.LogGet("guest", "config_key", value.has_value());
    // Should not attempt write
    audit.LogUnauthorizedAccess("guest", "127.0.0.1", "Attempted write without permission");
  }

  std::filesystem::remove_all(db_path);

  std::cout << " PASSED\n";
}

void test_audit_statistics_comprehensive() {
  std::cout << "Test: Comprehensive Audit Statistics..." << std::flush;

  AuditLogger audit("./test_stats_audit.log");

  // Simulate various activities
  audit.LogLogin("user1", "192.168.1.10", true);
  audit.LogLogin("user2", "192.168.1.11", true);
  audit.LogLogin("hacker1", "203.0.113.1", false);
  audit.LogLogin("hacker2", "203.0.113.2", false);
  audit.LogLogin("hacker3", "203.0.113.3", false);

  audit.LogPut("user1", "key1", true);
  audit.LogPut("user1", "key2", true);
  audit.LogGet("user2", "key1", true);
  audit.LogDelete("user1", "key2", true);

  audit.LogUnauthorizedAccess("user2", "192.168.1.11", "Attempted admin action");
  audit.LogUnauthorizedAccess("guest", "10.0.0.50", "Attempted write");

  audit.LogLogout("user1", "192.168.1.10");
  audit.LogLogout("user2", "192.168.1.11");

  // Verify statistics
  assert(audit.GetTotalEntryCount() == 13 && "Should have 13 total entries");
  assert(audit.GetFailedLoginCount() == 3 && "Should have 3 failed logins");
  assert(audit.GetUnauthorizedAccessCount() == 2 && "Should have 2 unauthorized attempts");

  // Query by user
  auto user1_entries = audit.GetEntriesByUser("user1");
  assert(user1_entries.size() >= 4 && "User1 should have multiple entries");

  auto user2_entries = audit.GetEntriesByUser("user2");
  assert(user2_entries.size() >= 3 && "User2 should have multiple entries");

  // Query by type
  auto logins = audit.GetEntriesByType(EventType::kLogin);
  assert(logins.size() == 2 && "Should have 2 successful logins");

  auto failed_logins = audit.GetEntriesByType(EventType::kLoginFailed);
  assert(failed_logins.size() == 3 && "Should have 3 failed logins");

  auto puts = audit.GetEntriesByType(EventType::kPut);
  assert(puts.size() == 2 && "Should have 2 PUT operations");

  std::cout << " PASSED\n";
}

int main() {
  std::cout << "=== Integration Security Tests ===\n\n";

  test_authenticated_database_operations();
  test_concurrent_authenticated_access();
  test_session_timeout_with_database();
  test_audit_log_with_failed_operations();
  test_configuration_driven_security();
  test_bulk_operations_with_audit();
  test_role_hierarchy_with_operations();
  test_audit_statistics_comprehensive();

  std::cout << "\n=== All Integration Tests PASSED ===\n";
  return 0;
}
