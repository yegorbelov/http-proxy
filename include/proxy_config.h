#pragma once

#include <string>
#include <vector>

namespace hp {

class ProxyConfig {
public:
  static ProxyConfig &instance();
  void load(const std::string &path);
  const std::string &listenHost() const { return listen_host_; }
  int listenPort() const { return listen_port_; }
  long long cacheMaxBytes() const;
  long long cacheMaxEntryBytes() const;
  int defaultTtlSec() const { return default_ttl_sec_; }
  const std::string &logPath() const { return log_path_; }
  bool adminReloadEnabled() const { return admin_reload_enabled_; }
  const std::vector<std::string> &allowUrlPatterns() const {
    return allow_url_patterns_;
  }
  const std::vector<std::string> &denyUrlPatterns() const {
    return deny_url_patterns_;
  }

private:
  ProxyConfig() = default;
  void set_defaults();
  void apply_line(const std::string &line, size_t nline);
  void parse_list_value(const std::string &s, char delim,
                        std::vector<std::string> &out);
  int parse_int(const std::string &v, int def, int min_v, int max_v);

  std::string listen_host_{"0.0.0.0"};
  int listen_port_{8080};
  int cache_max_size_mb_{256};
  int cache_max_entry_size_mb_{10};
  int default_ttl_sec_{300};
  int eviction_mode_{0};
  std::string log_path_{"proxy.log"};
  bool admin_reload_enabled_{true};
  std::vector<std::string> allow_url_patterns_;
  std::vector<std::string> deny_url_patterns_;
};

} // namespace hp
