#include "client_session_factory.h"

#include <thread>
#include <utility>

namespace hp {

ClientSessionFactory::ClientSessionFactory(UrlFilter &f, ResponseCache &c,
                                           std::shared_ptr<EventLog> log)
    : f_(f), c_(c), log_(std::move(log)) {}

void ClientSessionFactory::handle_connection(int client_fd) const {
  std::thread t([this, client_fd] {
    try {
      HttpClientSession s(client_fd, f_, c_, log_);
      s.run();
    } catch (...) {
    }
  });
  t.detach();
}

} // namespace hp
