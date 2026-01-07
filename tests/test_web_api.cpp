#include <catch2/catch_test_macros.hpp>
#include <core_engine/engine.hpp>
#include <httplib.h>

#include <filesystem>
#include <thread>
#include <chrono>

using namespace core_engine;
namespace fs = std::filesystem;

// Helper: Start server in background thread
class TestWebServer {
public:
  TestWebServer(const std::string& db_dir, int port)
      : db_dir_(db_dir), port_(port), running_(false) {
    // Clean up before test
    if (fs::exists(db_dir_)) {
      fs::remove_all(db_dir_);
    }
  }

  void Start() {
    server_thread_ = std::thread([this]() {
      Engine engine;
      auto status = engine.Open(db_dir_);
      if (!status.ok()) {
        return;
      }

      httplib::Server server;

      // PUT endpoint
      server.Post("/api/put", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("key") || !req.has_param("value")) {
          res.status = 400;
          res.set_content("Missing key or value", "text/plain");
          return;
        }
        const auto key = req.get_param_value("key");
        const auto value = req.get_param_value("value");
        const auto put_status = engine.Put(key, value);
        if (!put_status.ok()) {
          res.status = 500;
          res.set_content(put_status.ToString(), "text/plain");
          return;
        }
        res.set_content("OK", "text/plain");
      });

      // GET endpoint
      server.Get("/api/get", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("key")) {
          res.status = 400;
          res.set_content("Missing key", "text/plain");
          return;
        }
        const auto key = req.get_param_value("key");
        const auto value = engine.Get(key);
        if (!value.has_value()) {
          res.status = 404;
          res.set_content("NOT_FOUND", "text/plain");
          return;
        }
        res.set_content(*value, "text/plain");
      });

      // DELETE endpoint
      server.Post("/api/delete", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("key")) {
          res.status = 400;
          res.set_content("Missing key", "text/plain");
          return;
        }
        const auto key = req.get_param_value("key");
        const auto delete_status = engine.Delete(key);
        if (!delete_status.ok()) {
          res.status = 500;
          res.set_content(delete_status.ToString(), "text/plain");
          return;
        }
        res.set_content("OK", "text/plain");
      });

      // STATS endpoint
      server.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
        const auto stats = engine.GetStats();
        std::ostringstream json;
        json << "{"
             << "\"memtable_size_bytes\":" << stats.memtable_size_bytes << ","
             << "\"total_puts\":" << stats.total_puts << ","
             << "\"total_gets\":" << stats.total_gets
             << "}";
        res.set_content(json.str(), "application/json");
      });

      running_ = true;
      server.listen("127.0.0.1", port_);
    });

    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  void Stop() {
    // Server stops when thread ends
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
    
    // Clean up after test
    if (fs::exists(db_dir_)) {
      fs::remove_all(db_dir_);
    }
  }

  bool IsRunning() const { return running_; }

private:
  std::string db_dir_;
  int port_;
  bool running_;
  std::thread server_thread_;
};

TEST_CASE("Web API: PUT and GET", "[web][api][.]") {
  const std::string db_dir = "./test_web_api_put_get";
  const int port = 9001;

  TestWebServer server(db_dir, port);
  server.Start();

  httplib::Client client("127.0.0.1", port);

  SECTION("PUT a key-value pair") {
    httplib::Params params;
    params.emplace("key", "test_key");
    params.emplace("value", "test_value");

    auto res = client.Post("/api/put", params);
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(res->body == "OK");
  }

  SECTION("GET an existing key") {
    // First PUT
    httplib::Params params;
    params.emplace("key", "my_key");
    params.emplace("value", "my_value");
    client.Post("/api/put", params);

    // Then GET
    auto res = client.Get("/api/get?key=my_key");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(res->body == "my_value");
  }

  SECTION("GET a non-existent key returns 404") {
    auto res = client.Get("/api/get?key=nonexistent");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 404);
    REQUIRE(res->body == "NOT_FOUND");
  }

  server.Stop();
}

TEST_CASE("Web API: DELETE", "[web][api][.]") {
  const std::string db_dir = "./test_web_api_delete";
  const int port = 9002;

  TestWebServer server(db_dir, port);
  server.Start();

  httplib::Client client("127.0.0.1", port);

  SECTION("DELETE an existing key") {
    // PUT
    httplib::Params put_params;
    put_params.emplace("key", "deleteme");
    put_params.emplace("value", "value");
    client.Post("/api/put", put_params);

    // DELETE
    httplib::Params del_params;
    del_params.emplace("key", "deleteme");
    auto del_res = client.Post("/api/delete", del_params);
    REQUIRE(del_res != nullptr);
    REQUIRE(del_res->status == 200);

    // Verify deleted
    auto get_res = client.Get("/api/get?key=deleteme");
    REQUIRE(get_res->status == 404);
  }

  server.Stop();
}

TEST_CASE("Web API: STATS", "[web][api][.]") {
  const std::string db_dir = "./test_web_api_stats";
  const int port = 9003;

  TestWebServer server(db_dir, port);
  server.Start();

  httplib::Client client("127.0.0.1", port);

  SECTION("GET stats after operations") {
    // Insert some data
    for (int i = 0; i < 10; i++) {
      httplib::Params params;
      params.emplace("key", "key_" + std::to_string(i));
      params.emplace("value", "value_" + std::to_string(i));
      client.Post("/api/put", params);
    }

    // Get stats
    auto res = client.Get("/api/stats");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(res->headers.find("Content-Type")->second == "application/json");

    // Parse JSON (simple check for expected fields)
    REQUIRE(res->body.find("\"total_puts\":10") != std::string::npos);
    REQUIRE(res->body.find("\"memtable_size_bytes\"") != std::string::npos);
  }

  server.Stop();
}

TEST_CASE("Web API: Batch operations", "[web][api][integration][.]") {
  const std::string db_dir = "./test_web_api_batch";
  const int port = 9004;

  TestWebServer server(db_dir, port);
  server.Start();

  httplib::Client client("127.0.0.1", port);

  SECTION("Insert 100 entries") {
    const int count = 100;
    
    for (int i = 0; i < count; i++) {
      httplib::Params params;
      params.emplace("key", "batch_" + std::to_string(i));
      params.emplace("value", "data_" + std::to_string(i));
      auto res = client.Post("/api/put", params);
      REQUIRE(res != nullptr);
      REQUIRE(res->status == 200);
    }

    // Verify stats
    auto stats_res = client.Get("/api/stats");
    REQUIRE(stats_res->body.find("\"total_puts\":" + std::to_string(count)) != std::string::npos);

    // Verify a few random entries
    auto res1 = client.Get("/api/get?key=batch_0");
    REQUIRE(res1->body == "data_0");

    auto res2 = client.Get("/api/get?key=batch_50");
    REQUIRE(res2->body == "data_50");

    auto res3 = client.Get("/api/get?key=batch_99");
    REQUIRE(res3->body == "data_99");
  }

  server.Stop();
}

TEST_CASE("Web API: Error handling", "[web][api][.]") {
  const std::string db_dir = "./test_web_api_errors";
  const int port = 9005;

  TestWebServer server(db_dir, port);
  server.Start();

  httplib::Client client("127.0.0.1", port);

  SECTION("PUT without key returns 400") {
    httplib::Params params;
    params.emplace("value", "only_value");
    auto res = client.Post("/api/put", params);
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
    REQUIRE(res->body == "Missing key or value");
  }

  SECTION("PUT without value returns 400") {
    httplib::Params params;
    params.emplace("key", "only_key");
    auto res = client.Post("/api/put", params);
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
    REQUIRE(res->body == "Missing key or value");
  }

  SECTION("GET without key returns 400") {
    auto res = client.Get("/api/get");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
    REQUIRE(res->body == "Missing key");
  }

  SECTION("DELETE without key returns 400") {
    httplib::Params params;
    auto res = client.Post("/api/delete", params);
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
    REQUIRE(res->body == "Missing key");
  }

  server.Stop();
}

TEST_CASE("Web API: Special characters in keys and values", "[web][api][.]") {
  const std::string db_dir = "./test_web_api_special";
  const int port = 9006;

  TestWebServer server(db_dir, port);
  server.Start();

  httplib::Client client("127.0.0.1", port);

  SECTION("Keys with special characters") {
    httplib::Params params;
    params.emplace("key", "user:123:session");
    params.emplace("value", "active");
    auto put_res = client.Post("/api/put", params);
    REQUIRE(put_res->status == 200);

    auto get_res = client.Get("/api/get?key=user%3A123%3Asession");
    REQUIRE(get_res->status == 200);
    REQUIRE(get_res->body == "active");
  }

  SECTION("Values with JSON") {
    httplib::Params params;
    params.emplace("key", "user_data");
    params.emplace("value", R"({"name":"Alice","age":30})");
    auto put_res = client.Post("/api/put", params);
    REQUIRE(put_res->status == 200);

    auto get_res = client.Get("/api/get?key=user_data");
    REQUIRE(get_res->status == 200);
    REQUIRE(get_res->body == R"({"name":"Alice","age":30})");
  }

  server.Stop();
}
