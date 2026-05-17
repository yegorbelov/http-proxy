#include "proxy_config.h"
#include "response_cache.h"
#include "test_helpers.hpp"

#include <iostream>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: scenario_cache_demo <temp.ini>\n";
    return 1;
  }
  std::string ini_path = argv[1];
  if (ini_path == "--demo-create") {
    ini_path = make_temp_file_path();
    if (ini_path.empty()) {
      std::cerr << "mkstemp failed\n";
      return 2;
    }
    if (!write_file(ini_path, "cache_max_size_mb=1\n"
                              "cache_max_entry_size_mb=1\n"
                              "default_ttl_sec=600\n")) {
      std::cerr << "write ini failed\n";
      return 3;
    }
    std::cerr << "ini=" << ini_path << '\n';
  }

  hp::ProxyConfig::instance().load(ini_path);
  hp::ResponseCache cache;
  const std::string key = "GET http://demo/cache-key";
  hp::CacheEntry e;
  e.status_code = 200;
  e.body = "demo-body";
  e.headers["Content-Type"] = "text/plain";

  cache.store(key, std::move(e));
  const auto *hit = cache.lookup(key);
  std::cout << (hit ? "lookup_hit" : "lookup_miss") << '\n';
  const auto *hit2 = cache.lookup(key);
  std::cout << (hit2 ? "second_hit" : "second_miss") << '\n';
  return 0;
}
