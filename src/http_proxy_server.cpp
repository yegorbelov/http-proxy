#include "http_proxy_server.h"
#include "proxy_config.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

hp::HttpProxyServer* g_srv = nullptr;
void on_sigint(int) {
    if (g_srv) g_srv->stop();
}

}  // namespace

namespace hp {

void HttpProxyServer::stop() {
    run_flag_ = false;
    if (lsock_ >= 0) {
        (void)shutdown(lsock_, SHUT_RDWR);
    }
}

int HttpProxyServer::bind_listen() {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        std::cerr << "pks2-http-proxy: socket() failed: " << std::strerror(errno) << std::endl;
        return -1;
    }
    {
        int one = 1;
        (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, static_cast<socklen_t>(sizeof one));
    }
    sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<std::uint16_t>(ProxyConfig::instance().listenPort()));
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    {
        const std::string& h = ProxyConfig::instance().listenHost();
        if (h != "0.0.0.0" && h != "*") {
            if (inet_pton(AF_INET, h.c_str(), &a.sin_addr) != 1) {
                a.sin_addr.s_addr = htonl(INADDR_ANY);
            }
        }
    }
    if (::bind(s, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
        std::cerr << "pks2-http-proxy: bind() failed: " << std::strerror(errno) << std::endl;
        (void)close(s);
        return -1;
    }
    if (::listen(s, 64) != 0) {
        std::cerr << "pks2-http-proxy: listen() failed: " << std::strerror(errno) << std::endl;
        (void)close(s);
        return -1;
    }
    return s;
}

HttpProxyServer::HttpProxyServer()
    : filter_(
        ProxyConfig::instance().allowUrlPatterns(),
        ProxyConfig::instance().denyUrlPatterns()
    ) {
    log_ = std::make_shared<EventLog>(ProxyConfig::instance().logPath());
    session_factory_ = std::make_unique<ClientSessionFactory>(filter_, cache_, log_);
    g_srv = this;
    (void)std::signal(SIGINT, on_sigint);
}

int HttpProxyServer::run() {
    lsock_ = bind_listen();
    if (lsock_ < 0) {
        g_srv = nullptr;
        return 1;
    }
    std::cerr << "pks2-http-proxy: listening " << ProxyConfig::instance().listenHost() << ":"
              << ProxyConfig::instance().listenPort() << std::endl;
    for (;;) {
        if (!run_flag_.load()) {
            break;
        }
        int c = ::accept(lsock_, nullptr, nullptr);
        if (c < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (!run_flag_.load()) {
                break;
            }
            g_srv = nullptr;
            (void)close(lsock_);
            return 1;
        }
        session_factory_->handle_connection(c);
    }
    if (lsock_ >= 0) {
        (void)close(lsock_);
    }
    lsock_ = -1;
    g_srv = nullptr;
    return 0;
}

}  // namespace hp
