#include "ieviction_policy.h"

namespace hp {

void LruEvictionPolicy::touch(const std::string &key) {
  auto found = it_.find(key);
  if (found != it_.end())
    lru_order_.erase(found->second);
  lru_order_.push_back(key);
  auto pos = std::prev(lru_order_.end());
  it_[key] = pos;
}

void LruEvictionPolicy::on_remove(const std::string &key) {
  auto found = it_.find(key);
  if (found == it_.end())
    return;
  lru_order_.erase(found->second);
  it_.erase(found);
}

std::string LruEvictionPolicy::pick_lru() const {
  if (lru_order_.empty())
    return {};
  return lru_order_.front();
}

void LruEvictionPolicy::clear() {
  lru_order_.clear();
  it_.clear();
}

} // namespace hp
