#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace hp {

class EventLog {
public:
  EventLog(const std::string &path, bool to_stderr = true);
  void info(const std::string &m);
  void block(const std::string &url);
  void cache_hit(const std::string &key);
  void cache_miss(const std::string &key);
  void eviction(const std::string &what);

private:
  void write_line(const std::string &s);
  std::ofstream file_;
  std::mutex mutex_;
  bool to_stderr_;
};

} // namespace hp
