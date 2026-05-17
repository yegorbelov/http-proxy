#include "event_log.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace hp {

static std::string ts() {
  const auto t = std::time(nullptr);
  const std::tm *l = std::localtime(&t);
  std::ostringstream o;
  o << std::put_time(l, "%Y-%m-%d %H:%M:%S");
  return o.str();
}

EventLog::EventLog(const std::string &path, bool to_stderr)
    : to_stderr_(to_stderr) {
  file_.open(path, std::ios::out | std::ios::app);
}

void EventLog::write_line(const std::string &s) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_.is_open())
    file_ << s << std::endl;
  if (to_stderr_)
    std::cerr << s << std::endl;
}

void EventLog::info(const std::string &m) {
  write_line("[" + ts() + "] [info] " + m);
}
void EventLog::block(const std::string &url) {
  write_line("[" + ts() + "] [block] " + url);
}
void EventLog::cache_hit(const std::string &key) {
  write_line("[" + ts() + "] [cache] HIT  " + key);
}
void EventLog::cache_miss(const std::string &key) {
  write_line("[" + ts() + "] [cache] MISS " + key);
}
void EventLog::eviction(const std::string &what) {
  write_line("[" + ts() + "] [evict] " + what);
}

} // namespace hp
