#pragma once

#include "http_client_session.h"
#include "event_log.h"
#include "response_cache.h"
#include "url_filter.h"

#include <memory>

namespace hp {

class ClientSessionFactory {
   public:
    ClientSessionFactory(UrlFilter& f, ResponseCache& c, std::shared_ptr<EventLog> log);
    void handle_connection(int client_fd) const;

   private:
    UrlFilter& f_;
    ResponseCache& c_;
    std::shared_ptr<EventLog> log_;
};

}  // namespace hp
