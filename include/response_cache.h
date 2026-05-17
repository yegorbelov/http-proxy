#pragma once

#include "ieviction_policy.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace hp {

struct CacheEntry {
  int status_code{200};
  std::string body;
  std::map<std::string, std::string> headers;
  long long expires_at_unix{0};
};

class ResponseCache {
public:
  explicit ResponseCache(
      std::unique_ptr<ICacheEvictionPolicy> policy = nullptr);
  void set_policy(std::unique_ptr<ICacheEvictionPolicy> p);
  const CacheEntry *lookup(const std::string &key);

  void store(const std::string &key, CacheEntry e);
  void invalidate_expired();
  void evict_until_fit(long long need_bytes);
  void touch_on_hit(const std::string &key);

private:
  void ensure_space(long long additional);

  mutable std::mutex mu_;
  std::map<std::string, CacheEntry> entries_;
  long long total_bytes_{0};
  std::unique_ptr<ICacheEvictionPolicy> policy_;
  LruEvictionPolicy *lru_{nullptr};
};

} // namespace hp
