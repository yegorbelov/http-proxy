#pragma once

#include <memory>
#include <string>

namespace hp {

class EventLog {
   public:
    explicit EventLog(const std::string& path, bool to_stderr = true);
    ~EventLog();
    void info(const std::string& m);
    void block(const std::string& url);
    void cache_hit(const std::string& key);
    void cache_miss(const std::string& key);
    void eviction(const std::string& what);

   private:
    class Impl;
    std::unique_ptr<Impl> p_;
};

}  // namespace hp
