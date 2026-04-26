#include "ieviction_policy.h"

namespace hp {

void LruEvictionPolicy::touch(const std::string& key) {
    auto f = it_.find(key);
    if (f != it_.end()) lru_order_.erase(f->second);
    lru_order_.push_back(key);
    auto it = std::prev(lru_order_.end());
    it_[key] = it;
}

void LruEvictionPolicy::on_remove(const std::string& key) {
    auto f = it_.find(key);
    if (f == it_.end()) return;
    lru_order_.erase(f->second);
    it_.erase(f);
}

std::string LruEvictionPolicy::pick_lru() const {
    if (lru_order_.empty()) return {};
    return lru_order_.front();
}

void LruEvictionPolicy::clear() {
    lru_order_.clear();
    it_.clear();
}

}  // namespace hp
