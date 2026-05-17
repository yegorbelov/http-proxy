#include "proxy_config.h"

#include <iostream>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: scenario_config_dump <path.ini>\n";
    return 1;
  }
  hp::ProxyConfig::instance().load(argv[1]);
  const auto &c = hp::ProxyConfig::instance();
  std::cout << "listen_host=" << c.listenHost() << '\n';
  std::cout << "listen_port=" << c.listenPort() << '\n';
  std::cout << "cache_max_bytes=" << c.cacheMaxBytes() << '\n';
  std::cout << "cache_max_entry_bytes=" << c.cacheMaxEntryBytes() << '\n';
  std::cout << "default_ttl_sec=" << c.defaultTtlSec() << '\n';
  std::cout << "log_path=" << c.logPath() << '\n';
  std::cout << "admin_reload_enabled="
            << (c.adminReloadEnabled() ? "true" : "false") << '\n';
  std::cout << "allow_url_patterns:";
  for (size_t i = 0; i < c.allowUrlPatterns().size(); ++i) {
    if (i)
      std::cout << ';';
    std::cout << c.allowUrlPatterns()[i];
  }
  std::cout << '\n';
  std::cout << "deny_url_patterns:";
  for (size_t i = 0; i < c.denyUrlPatterns().size(); ++i) {
    if (i)
      std::cout << ';';
    std::cout << c.denyUrlPatterns()[i];
  }
  std::cout << '\n';
  return 0;
}
