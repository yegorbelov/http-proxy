#include "response_cache.h"
#include "proxy_config.h"

#include "ieviction_policy.h"

#include <cstdint>
#include <ctime>
#include <string>
#include <utility>

namespace hp {

static std::int64_t now_unix() { return static_cast<std::int64_t>(std::time(nullptr)); }

static std::int64_t entry_footprint(const std::string& k, const CacheEntry& e) {
    return static_cast<std::int64_t>(e.body.size() + 64 + k.size() * 2);
}

ResponseCache::ResponseCache(std::unique_ptr<ICacheEvictionPolicy> policy) { set_policy(std::move(policy)); }

void ResponseCache::set_policy(std::unique_ptr<ICacheEvictionPolicy> p) {
    std::lock_guard<std::mutex> lk(mu_);
    lru_ = nullptr;
    if (p) {
        policy_ = std::move(p);
        lru_ = dynamic_cast<LruEvictionPolicy*>(policy_.get());
    } else {
        auto l = std::make_unique<LruEvictionPolicy>();
        lru_ = l.get();
        policy_ = std::move(l);
    }
    if (lru_) lru_->clear();
    entries_.clear();
    total_bytes_ = 0;
}

const CacheEntry* ResponseCache::lookup(const std::string& key) {
    std::lock_guard<std::mutex> lk(mu_);
    auto f = entries_.find(key);
    if (f == entries_.end()) {
        return nullptr;
    }
    const std::int64_t t = now_unix();
    if (f->second.expires_at_unix != 0 && t > f->second.expires_at_unix) {
        if (lru_) lru_->on_remove(key);
        total_bytes_ -= entry_footprint(f->first, f->second);
        if (total_bytes_ < 0) total_bytes_ = 0;
        entries_.erase(f);
        return nullptr;
    }
    if (lru_) lru_->touch(key);
    return &f->second;
}

void ResponseCache::invalidate_expired() {
    std::lock_guard<std::mutex> lk(mu_);
    const std::int64_t t = now_unix();
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.expires_at_unix != 0 && t > it->second.expires_at_unix) {
            if (lru_) lru_->on_remove(it->first);
            total_bytes_ -= entry_footprint(it->first, it->second);
            it = entries_.erase(it);
        } else
            ++it;
    }
    if (total_bytes_ < 0) total_bytes_ = 0;
}

void ResponseCache::evict_until_fit(std::int64_t need_bytes) {
    std::lock_guard<std::mutex> lk(mu_);
    const std::int64_t cap = ProxyConfig::instance().cacheMaxBytes();
    while (total_bytes_ + need_bytes > cap && lru_ && !entries_.empty()) {
        std::string v = lru_->pick_lru();
        if (v.empty()) break;
        auto f = entries_.find(v);
        if (f == entries_.end()) {
            lru_->on_remove(v);
            continue;
        }
        lru_->on_remove(f->first);
        total_bytes_ -= entry_footprint(f->first, f->second);
        entries_.erase(f);
    }
    if (total_bytes_ < 0) total_bytes_ = 0;
}

void ResponseCache::ensure_space(std::int64_t additional) { evict_until_fit(additional); }

void ResponseCache::touch_on_hit(const std::string& key) { (void)lookup(key); }

void ResponseCache::store(const std::string& key, CacheEntry e) {
    std::lock_guard<std::mutex> lk(mu_);
    const std::int64_t maxE = ProxyConfig::instance().cacheMaxEntryBytes();
    if (static_cast<std::int64_t>(e.body.size()) > maxE) return;

    auto it = entries_.find(key);
    if (it != entries_.end()) {
        if (lru_) lru_->on_remove(key);
        total_bytes_ -= entry_footprint(it->first, it->second);
        if (total_bytes_ < 0) total_bytes_ = 0;
        entries_.erase(it);
    }

    e.expires_at_unix = now_unix() + static_cast<std::int64_t>(ProxyConfig::instance().defaultTtlSec());
    const std::int64_t add = entry_footprint(key, e);
    const std::int64_t cap = ProxyConfig::instance().cacheMaxBytes();

    while (total_bytes_ + add > cap && lru_ && !entries_.empty()) {
        const std::string v = lru_->pick_lru();
        if (v.empty()) break;
        if (v == key) break;
        auto f = entries_.find(v);
        if (f == entries_.end()) {
            lru_->on_remove(v);
            continue;
        }
        lru_->on_remove(f->first);
        total_bytes_ -= entry_footprint(f->first, f->second);
        entries_.erase(f);
    }
    if (total_bytes_ + add > cap) return;

    entries_.emplace(key, std::move(e));
    if (lru_) lru_->touch(key);
    total_bytes_ += add;
}

}  // namespace hp
