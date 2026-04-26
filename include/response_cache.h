#pragma once

#include "ieviction_policy.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace hp {

struct CacheEntry {
    int status_code{200};
    std::string body;
    std::map<std::string, std::string> headers;
    std::int64_t expires_at_unix{0};
};

class ResponseCache {
   public:
    explicit ResponseCache(std::unique_ptr<ICacheEvictionPolicy> policy = nullptr);
    void set_policy(std::unique_ptr<ICacheEvictionPolicy> p);
    const CacheEntry* lookup(const std::string& key);

    void store(const std::string& key, CacheEntry e);
    void invalidate_expired();
    void evict_until_fit(std::int64_t need_bytes);
    void touch_on_hit(const std::string& key);

   private:
    void ensure_space(std::int64_t additional);

    mutable std::mutex mu_;
    std::map<std::string, CacheEntry> entries_;
    std::int64_t total_bytes_{0};
    std::unique_ptr<ICacheEvictionPolicy> policy_;
    LruEvictionPolicy* lru_{nullptr};
};

}
