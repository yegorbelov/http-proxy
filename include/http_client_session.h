#pragma once

#include "event_log.h"
#include "response_cache.h"
#include "url_filter.h"

#include <cstdint>
#include <memory>
#include <string>

namespace hp {

class HttpClientSession {
   public:
    HttpClientSession(int client_fd, UrlFilter& f, ResponseCache& c, std::shared_ptr<EventLog> log);
    void run();
   private:
    void send_text(int code, const std::string& text);
    void send_buffer(const std::string& s);
    bool read_http_message(std::string& whole);
    void process_parsed(const std::string& raw_request);

    int client_fd_{-1};
    UrlFilter& filter_;
    ResponseCache& cache_;
    std::shared_ptr<EventLog> log_;
};

}  // namespace hp
