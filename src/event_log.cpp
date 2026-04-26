#include "event_log.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace hp {

class EventLog::Impl {
   public:
    std::ofstream f_;
    std::mutex m_;
    bool e_{true};
    explicit Impl(const std::string& path, bool to_stderr) : e_(to_stderr) {
        f_.open(path, std::ios::out | std::ios::app);
    }
    void line(const std::string& s) {
        const std::lock_guard<std::mutex> lk(m_);
        if (f_.is_open()) f_ << s << std::endl;
        if (e_) std::cerr << s << std::endl;
    }
};

static std::string ts() {
    const auto t = std::time(nullptr);
    const std::tm* const l = std::localtime(&t);
    std::ostringstream o;
    o << std::put_time(l, "%Y-%m-%d %H:%M:%S");
    return o.str();
}

EventLog::EventLog(const std::string& path, bool to_stderr) : p_(new Impl(path, to_stderr)) {}
EventLog::~EventLog() = default;

void EventLog::info(const std::string& m) { p_->line("[" + ts() + "] [info] " + m); }
void EventLog::block(const std::string& url) { p_->line("[" + ts() + "] [block] " + url); }
void EventLog::cache_hit(const std::string& key) { p_->line("[" + ts() + "] [cache] HIT  " + key); }
void EventLog::cache_miss(const std::string& key) { p_->line("[" + ts() + "] [cache] MISS " + key); }
void EventLog::eviction(const std::string& what) { p_->line("[" + ts() + "] [evict] " + what); }

}  // namespace hp
