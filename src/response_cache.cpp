#include "response_cache.h"
#include "ieviction_policy.h"
#include "proxy_config.h"

#include <ctime>
#include <string>
#include <utility>

namespace hp {

long long now_unix() { return (long long)std::time(nullptr); }

long long entry_footprint(const std::string &k, const CacheEntry &e) {
  return (long long)(e.body.size() + 64 + k.size() * 2);
}

ResponseCache::ResponseCache(std::unique_ptr<ICacheEvictionPolicy> policy) {
  set_policy(std::move(policy));
}

void ResponseCache::set_policy(std::unique_ptr<ICacheEvictionPolicy> p) {
  std::lock_guard<std::mutex> lock(mu_);
  lru_ = nullptr;
  if (p) {
    policy_ = std::move(p);
    lru_ = dynamic_cast<LruEvictionPolicy *>(policy_.get());
  } else {
    auto l = std::make_unique<LruEvictionPolicy>();
    lru_ = l.get();
    policy_ = std::move(l);
  }
  if (lru_)
    lru_->clear();
  entries_.clear();
  total_bytes_ = 0;
}

const CacheEntry *ResponseCache::lookup(const std::string &key) {
  std::lock_guard<std::mutex> lock(mu_);
  auto f = entries_.find(key);
  if (f == entries_.end()) {
    return nullptr;
  }
  const long long t = now_unix();
  if (f->second.expires_at_unix != 0 && t > f->second.expires_at_unix) {
    if (lru_)
      lru_->on_remove(key);
    total_bytes_ -= entry_footprint(f->first, f->second);
    if (total_bytes_ < 0)
      total_bytes_ = 0;
    entries_.erase(f);
    return nullptr;
  }
  if (lru_)
    lru_->touch(key);
  return &f->second;
}

void ResponseCache::invalidate_expired() {
  std::lock_guard<std::mutex> lock(mu_);
  const long long t = now_unix();
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->second.expires_at_unix != 0 && t > it->second.expires_at_unix) {
      if (lru_)
        lru_->on_remove(it->first);
      total_bytes_ -= entry_footprint(it->first, it->second);
      it = entries_.erase(it);
    } else
      ++it;
  }
  if (total_bytes_ < 0)
    total_bytes_ = 0;
}

void ResponseCache::evict_until_fit(long long need_bytes) {
  std::lock_guard<std::mutex> lock(mu_);
  const long long cap = ProxyConfig::instance().cacheMaxBytes();
  while (total_bytes_ + need_bytes > cap && lru_ && !entries_.empty()) {
    std::string v = lru_->pick_lru();
    if (v.empty())
      break;
    auto f = entries_.find(v);
    if (f == entries_.end()) {
      lru_->on_remove(v);
      continue;
    }
    lru_->on_remove(f->first);
    total_bytes_ -= entry_footprint(f->first, f->second);
    entries_.erase(f);
  }
  if (total_bytes_ < 0)
    total_bytes_ = 0;
}

void ResponseCache::ensure_space(long long additional) {
  evict_until_fit(additional);
}

void ResponseCache::touch_on_hit(const std::string &key) { lookup(key); }

void ResponseCache::store(const std::string &key, CacheEntry e) {
  std::lock_guard<std::mutex> lock(mu_);
  const long long maxE = ProxyConfig::instance().cacheMaxEntryBytes();
  if ((long long)e.body.size() > maxE)
    return;

  auto it = entries_.find(key);
  if (it != entries_.end()) {
    if (lru_)
      lru_->on_remove(key);
    total_bytes_ -= entry_footprint(it->first, it->second);
    if (total_bytes_ < 0)
      total_bytes_ = 0;
    entries_.erase(it);
  }

  e.expires_at_unix =
      now_unix() + (long long)ProxyConfig::instance().defaultTtlSec();
  const long long add = entry_footprint(key, e);
  const long long cap = ProxyConfig::instance().cacheMaxBytes();

  while (total_bytes_ + add > cap && lru_ && !entries_.empty()) {
    const std::string v = lru_->pick_lru();
    if (v.empty())
      break;
    if (v == key)
      break;
    auto f = entries_.find(v);
    if (f == entries_.end()) {
      lru_->on_remove(v);
      continue;
    }
    lru_->on_remove(f->first);
    total_bytes_ -= entry_footprint(f->first, f->second);
    entries_.erase(f);
  }
  if (total_bytes_ + add > cap)
    return;

  entries_.emplace(key, std::move(e));
  if (lru_)
    lru_->touch(key);
  total_bytes_ += add;
}

} // namespace hp
