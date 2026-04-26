#pragma once

#include <cstddef>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>

namespace hp {

class ICacheEvictionPolicy {
   public:
    virtual ~ICacheEvictionPolicy() = default;
    virtual void touch(const std::string& key) = 0;
    virtual void on_remove(const std::string& key) = 0;
};

class LruEvictionPolicy : public ICacheEvictionPolicy {
   public:
    LruEvictionPolicy() = default;
    void touch(const std::string& key) override;
    void on_remove(const std::string& key) override;
    std::string pick_lru() const;
    void clear();
    void adopt_order(const std::list<std::string>& lru) {
        lru_order_ = lru;
    }

   private:
    std::list<std::string> lru_order_;
    std::unordered_map<std::string, std::list<std::string>::iterator> it_;
};

}
