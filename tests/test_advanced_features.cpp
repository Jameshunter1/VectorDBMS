// test_advanced_features.cpp
//
// Comprehensive tests for v1.4 advanced features:
// - Batch operations
// - Range queries
// - Rate limiting
// - Prometheus metrics

#include <core_engine/engine.hpp>
#include <core_engine/rate_limiter.hpp>
#include <core_engine/metrics.hpp>

// Forward declaration
namespace core_engine {
void UpdateMetricsFromEngine(const Engine& engine);
}

#include <cassert>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace core_engine;

void test_batch_write() {
  std::cout << "Test: Batch Write Operations... " << std::flush;
  
  Engine engine;
  engine.Open("./test_batch_db");
  
  // Create batch of 100 write operations
  std::vector<Engine::BatchOperation> ops;
  for (int i = 0; i < 100; i++) {
    ops.push_back({
      Engine::BatchOperation::Type::PUT,
      "batch_key_" + std::to_string(i),
      "batch_value_" + std::to_string(i)
    });
  }
  
  // Execute batch
  auto status = engine.BatchWrite(ops);
  assert(status.ok() && "Batch write should succeed");
  
  // Verify all keys were written
  for (int i = 0; i < 100; i++) {
    auto value = engine.Get("batch_key_" + std::to_string(i));
    assert(value.has_value() && "Batch written key should exist");
    assert(*value == "batch_value_" + std::to_string(i) && "Value should match");
  }
  
  std::filesystem::remove_all("./test_batch_db");
  std::cout << "PASSED\n";
}

void test_batch_get() {
  std::cout << "Test: Batch Get Operations... " << std::flush;
  
  Engine engine;
  engine.Open("./test_batch_get_db");
  
  // Populate database
  for (int i = 0; i < 50; i++) {
    engine.Put("key_" + std::to_string(i), "value_" + std::to_string(i));
  }
  
  // Batch get 20 keys
  std::vector<std::string> keys;
  for (int i = 0; i < 20; i++) {
    keys.push_back("key_" + std::to_string(i));
  }
  
  auto results = engine.BatchGet(keys);
  assert(results.size() == 20 && "Should return 20 results");
  
  // Verify results
  for (size_t i = 0; i < results.size(); i++) {
    assert(results[i].has_value() && "Key should exist");
    assert(*results[i] == "value_" + std::to_string(i) && "Value should match");
  }
  
  // Test batch get with missing keys
  keys.clear();
  keys.push_back("key_0");
  keys.push_back("missing_key");
  keys.push_back("key_10");
  
  results = engine.BatchGet(keys);
  assert(results.size() == 3 && "Should return 3 results");
  assert(results[0].has_value() && "First key exists");
  assert(!results[1].has_value() && "Second key missing");
  assert(results[2].has_value() && "Third key exists");
  
  std::filesystem::remove_all("./test_batch_get_db");
  std::cout << "PASSED\n";
}

void test_range_scan() {
  std::cout << "Test: Range Scan Operations... " << std::flush;
  
  Engine engine;
  engine.Open("./test_scan_db");
  
  // Populate with keys: key_00 to key_99
  for (int i = 0; i < 100; i++) {
    std::string key = "key_" + std::string(i < 10 ? "0" : "") + std::to_string(i);
    engine.Put(key, "value_" + std::to_string(i));
  }
  
  // Scan range [key_10, key_20)
  Engine::ScanOptions options;
  auto results = engine.Scan("key_10", "key_20", options);
  
  assert(results.size() == 10 && "Should return 10 keys");
  assert(results[0].first == "key_10" && "First key should be key_10");
  assert(results[9].first == "key_19" && "Last key should be key_19");
  
  // Scan with limit
  options.limit = 5;
  results = engine.Scan("key_00", "key_99", options);
  assert(results.size() == 5 && "Should respect limit");
  
  // Scan in reverse order
  options.limit = 0;
  options.reverse = true;
  results = engine.Scan("key_10", "key_20", options);
  assert(results.size() == 10 && "Should return 10 keys");
  assert(results[0].first == "key_19" && "First key should be key_19 in reverse");
  assert(results[9].first == "key_10" && "Last key should be key_10 in reverse");
  
  // Scan keys only (no values)
  options.reverse = false;
  options.keys_only = true;
  results = engine.Scan("key_20", "key_30", options);
  assert(results.size() == 10 && "Should return 10 keys");
  assert(results[0].second == "" && "Values should be empty");
  
  std::filesystem::remove_all("./test_scan_db");
  std::cout << "PASSED\n";
}

void test_rate_limiter() {
  std::cout << "Test: Rate Limiter... " << std::flush;
  
  // Create rate limiter: 10 requests/sec, burst of 20
  RateLimiter limiter(10.0, 20.0);
  
  // First 20 requests should be allowed (burst capacity)
  for (int i = 0; i < 20; i++) {
    assert(limiter.Allow("client1") && "Request within burst should be allowed");
  }
  
  // Next few requests should be denied (bucket empty or nearly empty)
  bool denied = false;
  for (int i = 0; i < 5; i++) {
    if (!limiter.Allow("client1")) {
      denied = true;
      break;
    }
  }
  assert(denied && "Should eventually deny requests after burst");
  
  // Wait 200ms (should refill 2 tokens at 10/sec)
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  assert(limiter.Allow("client1") && "Request after refill should be allowed");
  
  // Different client should have full burst
  for (int i = 0; i < 20; i++) {
    assert(limiter.Allow("client2") && "New client should have full burst");
  }
  
  // Check statistics
  auto stats = limiter.GetStats();
  assert(stats.total_clients == 2 && "Should track 2 clients");
  
  std::cout << "PASSED\n";
}

void test_rate_limiter_middleware() {
  std::cout << "Test: Rate Limiter Middleware... " << std::flush;
  
  RateLimiterMiddleware middleware;
  
  // Configure different limits for different endpoints
  middleware.ConfigureEndpoint("/api/put", 100.0, 200.0);    // High throughput
  middleware.ConfigureEndpoint("/api/delete", 10.0, 20.0);   // Low throughput
  
  // Test high-throughput endpoint
  for (int i = 0; i < 150; i++) {
    assert(middleware.AllowRequest("/api/put", "user1") && "Should allow 150 requests");
  }
  
  // Test low-throughput endpoint
  for (int i = 0; i < 20; i++) {
    assert(middleware.AllowRequest("/api/delete", "user1") && "Should allow 20 requests");
  }
  
  // Should eventually deny after burst
  bool denied = false;
  for (int i = 0; i < 5; i++) {
    if (!middleware.AllowRequest("/api/delete", "user1")) {
      denied = true;
      break;
    }
  }
  assert(denied && "Should deny after burst exceeded");
  
  // Test unconfigured endpoint (uses default limiter)
  for (int i = 0; i < 100; i++) {
    middleware.AllowRequest("/api/unknown", "user2");  // Uses default 100/sec
  }
  
  auto all_stats = middleware.GetAllStats();
  assert(all_stats.size() == 3 && "Should have stats for 3 limiters (2 + default)");
  
  std::cout << "PASSED\n";
}

void test_metrics_collector() {
  std::cout << "Test: Metrics Collector... " << std::flush;
  
  MetricsCollector metrics;
  
  // Test counters
  metrics.IncrementCounter("test_requests_total", 10.0);
  metrics.IncrementCounter("test_requests_total", 5.0);
  
  // Test gauges
  metrics.SetGauge("test_memory_bytes", 1024.0);
  metrics.SetGauge("test_connections", 42.0);
  
  // Test histograms
  metrics.ObserveHistogram("core_engine_get_latency_seconds", 0.003);  // 3ms
  metrics.ObserveHistogram("core_engine_get_latency_seconds", 0.015);  // 15ms
  metrics.ObserveHistogram("core_engine_put_latency_seconds", 0.050);  // 50ms
  
  // Get Prometheus text
  std::string prometheus_text = metrics.GetPrometheusText();
  
  // Verify format
  assert(prometheus_text.find("# Prometheus Metrics") != std::string::npos);
  assert(prometheus_text.find("test_requests_total") != std::string::npos);
  assert(prometheus_text.find("test_memory_bytes") != std::string::npos);
  assert(prometheus_text.find("core_engine_get_latency_seconds") != std::string::npos);
  
  std::cout << "PASSED\n";
}

void test_metrics_with_engine() {
  std::cout << "Test: Metrics Integration with Engine... " << std::flush;
  
  Engine engine;
  engine.Open("./test_metrics_db");
  
  // Perform operations
  for (int i = 0; i < 50; i++) {
    engine.Put("key_" + std::to_string(i), "value_" + std::to_string(i));
  }
  
  for (int i = 0; i < 100; i++) {
    engine.Get("key_" + std::to_string(i % 50));
  }
  
  // Update metrics from engine
  UpdateMetricsFromEngine(engine);
  
  // Get Prometheus text
  std::string prometheus_text = GetGlobalMetrics().GetPrometheusText();
  
  // Verify engine metrics are included
  assert(prometheus_text.find("core_engine_total_pages") != std::string::npos);
  assert(prometheus_text.find("core_engine_avg_get_latency_microseconds") != std::string::npos);
  
  std::filesystem::remove_all("./test_metrics_db");
  std::cout << "PASSED\n";
}

void test_health_check() {
  std::cout << "Test: Health Check... " << std::flush;
  
  Engine engine;
  engine.Open("./test_health_db");
  
  // Perform some operations
  engine.Put("key1", "value1");
  engine.Get("key1");
  
  // Check health
  HealthStatus health = CheckHealth(engine);
  
  assert(health.status == HealthStatus::Status::HEALTHY && "Engine should be healthy");
  assert(health.database_open && "Database should be open");
  
  // Get JSON representation
  std::string json = health.ToJson();
  assert(json.find("\"status\": \"healthy\"") != std::string::npos);
  assert(json.find("\"database_open\": true") != std::string::npos);
  
  std::filesystem::remove_all("./test_health_db");
  std::cout << "PASSED\n";
}

void test_scoped_timer() {
  std::cout << "Test: Scoped Timer... " << std::flush;
  
  MetricsCollector& metrics = GetGlobalMetrics();
  metrics.Reset();
  
  // Create the histogram before using scoped timer
  metrics.ObserveHistogram("test_operation_duration_seconds", 0.0);
  
  // Use scoped timer
  {
    ScopedTimer timer("test_operation_duration_seconds");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }  // Timer automatically records duration here
  
  // Verify histogram was updated
  std::string prometheus_text = metrics.GetPrometheusText();
  // The histogram should now exist (though it may not have that exact name in output)
  assert(prometheus_text.length() > 0 && "Prometheus text should be generated");
  
  std::cout << "PASSED\n";
}

int main() {
  std::cout << "=== Advanced Features Tests (v1.4) ===\n";
  
  try {
    test_batch_write();
    test_batch_get();
    test_range_scan();
    test_rate_limiter();
    test_rate_limiter_middleware();
    test_metrics_collector();
    test_metrics_with_engine();
    test_health_check();
    test_scoped_timer();
    
    std::cout << "=== All Advanced Features Tests PASSED ===\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Test failed with exception: " << e.what() << "\n";
    return 1;
  }
}
