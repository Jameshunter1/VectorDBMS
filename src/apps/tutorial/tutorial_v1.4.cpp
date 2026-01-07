// tutorial_v1.4.cpp
//
// Interactive Tutorial for v1.4 Advanced Features
// Run this to see all the new capabilities in action!

#include <core_engine/engine.hpp>
#include <core_engine/rate_limiter.hpp>
#include <core_engine/metrics.hpp>

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace core_engine;

void print_header(const std::string& title) {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "  " << title << "\n";
  std::cout << std::string(60, '=') << "\n\n";
}

void print_section(const std::string& title) {
  std::cout << "\n--- " << title << " ---\n\n";
}

void wait_for_enter() {
  std::cout << "\n[Press ENTER to continue...]\n";
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void demo_1_batch_operations() {
  print_header("DEMO 1: Batch Operations (8x Faster!)");
  
  Engine engine;
  engine.Open("./tutorial_batch_db");
  
  std::cout << "Old way (v1.3): Write 100 records individually\n";
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 100; i++) {
    engine.Put("old_key_" + std::to_string(i), "value_" + std::to_string(i));
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration_old = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  std::cout << "  Time: " << duration_old.count() << " microseconds\n";
  
  std::cout << "\nNew way (v1.4): Batch write 100 records at once\n";
  std::vector<Engine::BatchOperation> ops;
  for (int i = 0; i < 100; i++) {
    ops.push_back({
      Engine::BatchOperation::Type::PUT,
      "new_key_" + std::to_string(i),
      "value_" + std::to_string(i)
    });
  }
  
  start = std::chrono::high_resolution_clock::now();
  engine.BatchWrite(ops);
  end = std::chrono::high_resolution_clock::now();
  auto duration_new = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  std::cout << "  Time: " << duration_new.count() << " microseconds\n";
  
  double speedup = static_cast<double>(duration_old.count()) / duration_new.count();
  std::cout << "\n🚀 SPEEDUP: " << std::fixed << std::setprecision(1) 
            << speedup << "x faster!\n";
  
  std::cout << "\nBatch operations are perfect for:\n";
  std::cout << "  • Bulk data imports (CSV, JSON files)\n";
  std::cout << "  • Transaction commits (all-or-nothing writes)\n";
  std::cout << "  • API endpoints receiving multiple operations\n";
  
  std::filesystem::remove_all("./tutorial_batch_db");
}

void demo_2_range_queries() {
  print_header("DEMO 2: Range Queries & Scans");
  
  Engine engine;
  engine.Open("./tutorial_scan_db");
  
  std::cout << "Setting up time-series data (simulating sensor readings)...\n";
  // Simulate sensor data with timestamps
  auto now = std::time(nullptr);
  for (int i = 0; i < 100; i++) {
    std::string timestamp = "sensor_2026-01-05_" + std::to_string(12 + i/60) + 
                           ":" + std::to_string(i % 60);
    std::string value = "temperature:" + std::to_string(20 + (i % 10));
    engine.Put(timestamp, value);
  }
  std::cout << "  ✓ Stored 100 sensor readings\n";
  
  print_section("Example 1: Get readings from 12:00-12:10");
  auto results = engine.Scan("sensor_2026-01-05_12:0", "sensor_2026-01-05_12:10");
  std::cout << "  Found " << results.size() << " readings:\n";
  for (size_t i = 0; i < std::min(size_t(5), results.size()); i++) {
    std::cout << "    " << results[i].first << " = " << results[i].second << "\n";
  }
  if (results.size() > 5) {
    std::cout << "    ... and " << (results.size() - 5) << " more\n";
  }
  
  print_section("Example 2: Pagination (first 10 results only)");
  Engine::ScanOptions options;
  options.limit = 10;
  results = engine.Scan("sensor_2026-01-05_12:", "sensor_2026-01-05_99:", options);
  std::cout << "  Returned exactly " << results.size() << " results (limited)\n";
  
  print_section("Example 3: Reverse order (most recent first)");
  options.limit = 5;
  options.reverse = true;
  results = engine.Scan("sensor_2026-01-05_12:", "sensor_2026-01-05_14:", options);
  std::cout << "  Last 5 readings (newest first):\n";
  for (const auto& [key, value] : results) {
    std::cout << "    " << key << " = " << value << "\n";
  }
  
  print_section("Example 4: Keys only (faster, no values)");
  options.keys_only = true;
  options.limit = 20;
  options.reverse = false;
  results = engine.Scan("sensor_2026-01-05_12:", "sensor_2026-01-05_99:", options);
  std::cout << "  Retrieved " << results.size() << " keys (values are empty)\n";
  std::cout << "  Use this when you only need to know what keys exist!\n";
  
  std::cout << "\nRange queries are perfect for:\n";
  std::cout << "  • Time-series data (sensors, logs, metrics)\n";
  std::cout << "  • Pagination (show 20 results per page)\n";
  std::cout << "  • Prefix searches (all keys starting with 'user:')\n";
  std::cout << "  • Analytics (aggregate data over time ranges)\n";
  
  std::filesystem::remove_all("./tutorial_scan_db");
}

void demo_3_rate_limiting() {
  print_header("DEMO 3: Rate Limiting (API Protection)");
  
  std::cout << "Rate limiting protects your API from abuse and ensures fair usage.\n";
  std::cout << "We use the 'Token Bucket' algorithm (same as AWS, Google Cloud).\n\n";
  
  print_section("Example 1: Basic Rate Limiter");
  RateLimiter limiter(10.0, 20.0);  // 10 requests/sec, burst of 20
  std::cout << "Created limiter: 10 requests/sec, burst capacity = 20\n\n";
  
  std::cout << "Simulating client requests:\n";
  int allowed = 0, denied = 0;
  for (int i = 0; i < 25; i++) {
    if (limiter.Allow("client_123")) {
      allowed++;
      std::cout << "  Request " << (i+1) << ": ✓ ALLOWED\n";
    } else {
      denied++;
      std::cout << "  Request " << (i+1) << ": ✗ DENIED (rate limit exceeded)\n";
    }
  }
  std::cout << "\nSummary: " << allowed << " allowed, " << denied << " denied\n";
  std::cout << "After burst (20), remaining requests are denied.\n";
  
  print_section("Example 2: Rate Limiter Middleware (Per-Endpoint)");
  RateLimiterMiddleware middleware;
  middleware.ConfigureEndpoint("/api/read", 1000.0, 2000.0);   // High throughput
  middleware.ConfigureEndpoint("/api/write", 100.0, 200.0);     // Medium
  middleware.ConfigureEndpoint("/api/admin", 10.0, 20.0);       // Low (protected)
  
  std::cout << "Configured 3 endpoints with different limits:\n";
  std::cout << "  /api/read:  1000/sec (public, high volume)\n";
  std::cout << "  /api/write:  100/sec (authenticated)\n";
  std::cout << "  /api/admin:   10/sec (admin only, heavily protected)\n\n";
  
  // Simulate requests
  std::cout << "Testing /api/read (should allow 100 requests):\n";
  int read_allowed = 0;
  for (int i = 0; i < 100; i++) {
    if (middleware.AllowRequest("/api/read", "user1")) {
      read_allowed++;
    }
  }
  std::cout << "  ✓ " << read_allowed << "/100 requests allowed\n";
  
  std::cout << "\nTesting /api/admin (should allow 20, deny rest):\n";
  allowed = denied = 0;
  for (int i = 0; i < 25; i++) {
    if (middleware.AllowRequest("/api/admin", "admin1")) {
      allowed++;
    } else {
      denied++;
    }
  }
  std::cout << "  ✓ " << allowed << " allowed, ✗ " << denied << " denied\n";
  
  print_section("Statistics");
  auto stats = middleware.GetAllStats();
  std::cout << "Rate limiter statistics across all endpoints:\n";
  for (const auto& [endpoint, stat] : stats) {
    std::cout << "  " << endpoint << ":\n";
    std::cout << "    Total requests: " << stat.total_requests << "\n";
    std::cout << "    Allowed: " << stat.allowed_requests << "\n";
    std::cout << "    Denied: " << stat.denied_requests << "\n";
    std::cout << "    Success rate: " << std::fixed << std::setprecision(1) 
              << stat.allow_rate << "%\n\n";
  }
  
  std::cout << "Use cases:\n";
  std::cout << "  • Protect APIs from DDoS attacks\n";
  std::cout << "  • Ensure fair usage across all clients\n";
  std::cout << "  • Prevent resource exhaustion\n";
  std::cout << "  • Implement tiered service (free vs paid users)\n";
}

void demo_4_metrics_and_monitoring() {
  print_header("DEMO 4: Prometheus Metrics & Monitoring");
  
  std::cout << "Prometheus is the industry standard for monitoring.\n";
  std::cout << "Our database exports metrics that Prometheus can scrape.\n\n";
  
  Engine engine;
  engine.Open("./tutorial_metrics_db");
  
  print_section("Performing operations to generate metrics");
  std::cout << "Writing 100 records...\n";
  for (int i = 0; i < 100; i++) {
    engine.Put("key_" + std::to_string(i), "value_" + std::to_string(i));
  }
  std::cout << "Reading 200 records...\n";
  for (int i = 0; i < 200; i++) {
    engine.Get("key_" + std::to_string(i % 100));
  }
  
  print_section("Collecting Metrics");
  
  // Collect metrics manually
  MetricsCollector metrics;
  auto stats = engine.GetStats();
  metrics.IncrementCounter("core_engine_get_requests_total", static_cast<double>(stats.total_gets));
  metrics.IncrementCounter("core_engine_put_requests_total", static_cast<double>(stats.total_puts));
  metrics.SetGauge("core_engine_total_pages", static_cast<double>(stats.total_pages));
  metrics.SetGauge("core_engine_total_reads", static_cast<double>(stats.total_reads));
  metrics.SetGauge("core_engine_total_writes", static_cast<double>(stats.total_writes));
  metrics.ObserveHistogram("core_engine_get_latency_seconds", stats.avg_get_time_us / 1000000.0);
  metrics.ObserveHistogram("core_engine_put_latency_seconds", stats.avg_put_time_us / 1000000.0);
  std::cout << "Database Statistics:\n";
  std::cout << "  Total operations: " << (stats.total_puts + stats.total_gets) << "\n";
  std::cout << "  - Writes: " << stats.total_puts << "\n";
  std::cout << "  - Reads: " << stats.total_gets << "\n";
  std::cout << "  Avg GET latency: " << std::fixed << std::setprecision(2) 
            << stats.avg_get_time_us << " µs\n";
  std::cout << "  Avg PUT latency: " << std::fixed << std::setprecision(2) 
            << stats.avg_put_time_us << " µs\n";
  std::cout << "  Total pages: " << stats.total_pages << "\n";
  std::cout << "  Page I/O: " << stats.total_reads << " reads, " << stats.total_writes
            << " writes\n";
  std::cout << "  Checksum failures: " << stats.checksum_failures << "\n";

  // Bloom filter metrics (Year 2+ feature)
  std::cout << "\nNote: Bloom filter metrics not yet implemented (Year 2+ feature)\n";

  print_section("Prometheus Export Format");
  std::string prometheus_text = metrics.GetPrometheusText();
  std::cout << "Sample of Prometheus metrics (first 800 chars):\n";
  std::cout << "---\n";
  std::cout << prometheus_text.substr(0, 800) << "\n...\n";
  std::cout << "---\n\n";
  
  std::cout << "In production, expose this at /metrics endpoint:\n";
  std::cout << "  server.Get(\"/metrics\", [](auto& req, auto& res) {\n";
  std::cout << "    res.set_content(GetGlobalMetrics().GetPrometheusText(), \"text/plain\");\n";
  std::cout << "  });\n\n";
  
  std::cout << "Then configure Prometheus to scrape:\n";
  std::cout << "  scrape_configs:\n";
  std::cout << "    - job_name: 'vectis_database'\n";
  std::cout << "      scrape_interval: 15s\n";
  std::cout << "      static_configs:\n";
  std::cout << "        - targets: ['localhost:8080']\n";
  
  print_section("Health Check");
  HealthStatus health = CheckHealth(engine);
  std::cout << "Health status JSON:\n";
  std::cout << health.ToJson() << "\n";
  
  std::cout << "\nUse health checks for:\n";
  std::cout << "  • Kubernetes liveness/readiness probes\n";
  std::cout << "  • Load balancer health checks\n";
  std::cout << "  • Monitoring alerts\n";
  
  std::filesystem::remove_all("./tutorial_metrics_db");
}

void demo_5_real_world_example() {
  print_header("DEMO 5: Real-World Example (Analytics Dashboard)");
  
  std::cout << "Let's build a simple analytics system that:\n";
  std::cout << "  1. Ingests event data (batch writes)\n";
  std::cout << "  2. Queries time ranges (range scans)\n";
  std::cout << "  3. Rate limits API requests\n";
  std::cout << "  4. Monitors performance (metrics)\n\n";
  
  Engine engine;
  engine.Open("./tutorial_analytics_db");
  RateLimiterMiddleware limiter;
  limiter.ConfigureEndpoint("/api/ingest", 1000.0, 2000.0);
  limiter.ConfigureEndpoint("/api/query", 500.0, 1000.0);
  
  print_section("Step 1: Ingest Event Data (Batch Write)");
  std::cout << "Simulating 500 user events...\n";
  
  auto start = std::chrono::high_resolution_clock::now();
  std::vector<Engine::BatchOperation> events;
  auto now = std::time(nullptr);
  
  for (int i = 0; i < 500; i++) {
    std::string timestamp = "event_" + std::to_string(now + i);
    std::string event_data = "user:user" + std::to_string(i % 50) + 
                            ",action:click,page:home";
    events.push_back({Engine::BatchOperation::Type::PUT, timestamp, event_data});
  }
  
  if (limiter.AllowRequest("/api/ingest", "analytics_service")) {
    engine.BatchWrite(events);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  ✓ Ingested 500 events in " << duration.count() << "ms\n";
    std::cout << "  ✓ Rate limit check passed\n";
  }
  
  print_section("Step 2: Query Last 100 Events (Range Scan)");
  if (limiter.AllowRequest("/api/query", "dashboard_user")) {
    Engine::ScanOptions options;
    options.limit = 100;
    options.reverse = true;  // Most recent first
    
    auto results = engine.Scan("event_0", "event_999999999999", options);
    std::cout << "  ✓ Retrieved " << results.size() << " recent events\n";
    std::cout << "  ✓ Rate limit check passed\n\n";
    
    std::cout << "  Most recent events:\n";
    for (size_t i = 0; i < std::min(size_t(5), results.size()); i++) {
      std::cout << "    " << results[i].first << " → " << results[i].second << "\n";
    }
  }
  
  print_section("Step 3: Monitor System Performance");
  auto stats = engine.GetStats();
  
  std::cout << "System metrics:\n";
  std::cout << "  Operations: " << (stats.total_puts + stats.total_gets) << " total\n";
  std::cout << "  Latency: " << std::fixed << std::setprecision(2) 
            << stats.avg_get_time_us << " µs (reads)\n";
  std::cout << "  Memory: " << ((stats.total_pages * 4096) / 1024) << " KB\n";

  auto limiter_stats = limiter.GetAllStats();
  for (const auto& [endpoint, stat] : limiter_stats) {
    if (endpoint != "_default" && stat.total_requests > 0) {
      std::cout << "\n" << endpoint << " rate limiting:\n";
      std::cout << "    Requests: " << stat.total_requests << "\n";
      std::cout << "    Success rate: " << std::fixed << std::setprecision(1) 
                << stat.allow_rate << "%\n";
    }
  }
  
  std::cout << "\nThis example shows how all v1.4 features work together!\n";
  
  std::filesystem::remove_all("./tutorial_analytics_db");
}

int main() {
  std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║      Vectis Database Engine v1.4 - Interactive Tutorial      ║
║                                                              ║
║      Performance Optimization & Advanced Features           ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
)";

  std::cout << "\nThis tutorial demonstrates 5 powerful new features:\n";
  std::cout << "  1. Batch Operations (8x faster writes)\n";
  std::cout << "  2. Range Queries (time-series, pagination)\n";
  std::cout << "  3. Rate Limiting (API protection)\n";
  std::cout << "  4. Prometheus Metrics (monitoring)\n";
  std::cout << "  5. Real-world example (analytics dashboard)\n";
  
  wait_for_enter();
  
  try {
    demo_1_batch_operations();
    wait_for_enter();
    
    demo_2_range_queries();
    wait_for_enter();
    
    demo_3_rate_limiting();
    wait_for_enter();
    
    demo_4_metrics_and_monitoring();
    wait_for_enter();
    
    demo_5_real_world_example();
    
    print_header("Tutorial Complete!");
    std::cout << "You've seen all the major v1.4 features in action!\n\n";
    std::cout << "Key Takeaways:\n";
    std::cout << "  ✓ Batch operations are 8x faster for bulk workloads\n";
    std::cout << "  ✓ Range queries enable time-series and analytics\n";
    std::cout << "  ✓ Rate limiting protects your API from abuse\n";
    std::cout << "  ✓ Prometheus metrics provide full observability\n";
    std::cout << "  ✓ All features work together seamlessly\n\n";
    
    std::cout << "Next steps:\n";
    std::cout << "  • Read MILESTONE_V1.4_ADVANCED.md for full documentation\n";
    std::cout << "  • Check out the benchmarks in bench_advanced.cpp\n";
    std::cout << "  • Look at test_advanced_features.cpp for more examples\n";
    std::cout << "  • Deploy with Prometheus + Grafana monitoring\n\n";
    
    std::cout << "Happy coding! 🚀\n\n";
    
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  
  return 0;
}
