#pragma once

#include "client_session_factory.h"
#include "event_log.h"
#include "response_cache.h"
#include "url_filter.h"

#include <atomic>
#include <memory>
#include <string>

namespace hp {

class HttpProxyServer {
   public:
    HttpProxyServer();
    int run();
    void stop();

   private:
    int bind_listen();
    int lsock_{-1};
    std::atomic_bool run_flag_{true};
    UrlFilter filter_;
    ResponseCache cache_{};
    std::shared_ptr<EventLog> log_{};
    std::unique_ptr<ClientSessionFactory> session_factory_{};
};

}  // namespace hp
