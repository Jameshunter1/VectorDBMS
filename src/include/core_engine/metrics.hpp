#pragma once

// core_engine/metrics.hpp
//
// Purpose:
// - Prometheus-compatible metrics for monitoring and observability.
// - Exposes performance, health, and business metrics.
//
// v1.4: Advanced Features

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace core_engine {

// Forward declarations
class Engine;

/**
 * PROMETHEUS METRICS EXPORTER
 *
 * Provides metrics in Prometheus text format for scraping.
 * https://prometheus.io/docs/instrumenting/exposition_formats/
 *
 * Metric Types:
 * - Counter: Monotonically increasing value (requests, errors).
 * - Gauge: Value that can go up/down (memory usage, active connections).
 * - Histogram: Distribution of values (latencies, sizes).
 * - Summary: Similar to histogram with quantiles.
 *
 * Example Prometheus query:
 *   rate(core_engine_requests_total[5m])  # Requests per second
 *   core_engine_memtable_size_bytes       # Current MemTable size
 *   histogram_quantile(0.95, core_engine_get_latency_seconds_bucket)  # P95 latency
 *
 * Integration:
 *   1. Call UpdateMetrics() periodically (e.g., every second).
 *   2. Expose /metrics endpoint that returns GetPrometheusText().
 *   3. Configure Prometheus to scrape your endpoint.
 */
class MetricsCollector {
public:
  MetricsCollector();

  // =========================================================================
  // COUNTER METRICS (monotonically increasing)
  // =========================================================================

  void IncrementCounter(const std::string& name, double value = 1.0);

  // =========================================================================
  // GAUGE METRICS (current value)
  // =========================================================================

  void SetGauge(const std::string& name, double value);

  // =========================================================================
  // HISTOGRAM METRICS (latency distribution)
  // =========================================================================

  void ObserveHistogram(const std::string& name, double value);

  // =========================================================================
  // PROMETHEUS EXPORT
  // =========================================================================

  /**
   * Get all metrics in Prometheus text format.
   * This is what /metrics endpoint should return.
   *
   * Example output:
   *   # HELP core_engine_requests_total Total number of requests
   *   # TYPE core_engine_requests_total counter
   *   core_engine_requests_total{operation="get"} 12345
   *   core_engine_requests_total{operation="put"} 6789
   */
  std::string GetPrometheusText() const;

  /**
   * Update all engine metrics (call this periodically).
   *
   * @param stats Engine statistics to export.
   */
  struct EngineStats;
  void UpdateFromEngineStats(const EngineStats& stats);

  /**
   * Reset all metrics (for testing).
   */
  void Reset();

private:
  mutable std::mutex mutex_;

  // Counters: operation -> count
  std::unordered_map<std::string, std::atomic<double>> counters_;

  // Gauges: metric_name -> current_value
  std::unordered_map<std::string, double> gauges_;

  // Histograms: metric_name -> buckets
  struct HistogramBucket {
    double upper_bound;
    std::uint64_t count;
  };
  std::unordered_map<std::string, std::vector<HistogramBucket>> histograms_;

  // Helper: Format counter for Prometheus
  std::string FormatCounter(const std::string& name, double value) const;

  // Helper: Format gauge for Prometheus
  std::string FormatGauge(const std::string& name, double value) const;

  // Helper: Format histogram for Prometheus
  std::string FormatHistogram(const std::string& name,
                              const std::vector<HistogramBucket>& buckets) const;
};

/**
 * GLOBAL METRICS INSTANCE
 *
 * Singleton for easy access from anywhere in the codebase.
 */
MetricsCollector& GetGlobalMetrics();

/**
 * SCOPED TIMER FOR AUTOMATIC LATENCY TRACKING
 *
 * Usage:
 *   void MyFunction() {
 *     ScopedTimer timer("my_function_duration_seconds");
 *     // ... do work ...
 *   }  // Automatically records duration on destruction
 */
class ScopedTimer {
public:
  explicit ScopedTimer(std::string metric_name);
  ~ScopedTimer();

  // Disable copy/move
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
  std::string metric_name_;
  std::chrono::steady_clock::time_point start_;
};

/**
 * HEALTH CHECK ENDPOINT
 *
 * Provides /health endpoint for load balancers and orchestrators.
 */
struct HealthStatus {
  enum class Status {
    HEALTHY,  // All systems operational
    DEGRADED, // Some non-critical issues
    UNHEALTHY // Critical failure
  };

  Status status = Status::HEALTHY;
  std::string message;

  // Component health
  bool database_open = false;
  bool wal_healthy = false;
  bool memtable_healthy = false;
  bool sstables_healthy = false;

  // Resource utilization
  double memory_usage_mb = 0.0;
  double disk_usage_mb = 0.0;
  std::size_t active_connections = 0;

  /**
   * Convert to JSON for /health endpoint.
   */
  std::string ToJson() const;
};

/**
 * Check overall system health.
 */
HealthStatus CheckHealth(const class Engine& engine);

} // namespace core_engine
