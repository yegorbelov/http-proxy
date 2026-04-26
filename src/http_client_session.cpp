#include "http_client_session.h"
#include "proxy_config.h"
#include "upstream_connector.h"

#include <cerrno>
#include <exception>
#include <sstream>
#include <string>
#include <unistd.h>

namespace hp {

namespace {

std::string make_full_url(const FetchRequest& r) {
    std::string u = "http://";
    u += r.target.host;
    if (r.target.port != 80) {
        u += ':';
        u += std::to_string(r.target.port);
    }
    u += r.target.path_and_query;
    return u;
}

void send_all(int fd, const void* p, size_t n) {
    const char* c = static_cast<const char*>(p);
    while (n) {
        const ssize_t w = ::write(fd, c, n);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        if (w == 0) {
            return;
        }
        c += static_cast<size_t>(w);
        n -= static_cast<size_t>(w);
    }
}

const char* reason_phrase(int c) {
    if (c == 200) return "OK";
    if (c == 400) return "Bad Request";
    if (c == 403) return "Forbidden";
    if (c == 404) return "Not Found";
    if (c == 500) return "Internal Server Error";
    if (c == 501) return "Not Implemented";
    if (c == 502) return "Bad Gateway";
    return "OK";
}

}  // namespace

HttpClientSession::HttpClientSession(int client_fd, UrlFilter& f, ResponseCache& c, std::shared_ptr<EventLog> log)
    : client_fd_(client_fd), filter_(f), cache_(c), log_(std::move(log)) {}

void HttpClientSession::send_text(int code, const std::string& text) {
    std::ostringstream o;
    o << "HTTP/1.1 " << code << ' ' << reason_phrase(code) << "\r\n";
    o << "Content-Type: text/plain; charset=utf-8\r\n";
    o << "Connection: close\r\n";
    o << "Content-Length: " << text.size() << "\r\n";
    o << "\r\n" << text;
    send_buffer(o.str());
}

void HttpClientSession::send_buffer(const std::string& s) { send_all(client_fd_, s.data(), s.size()); }

bool HttpClientSession::read_http_message(std::string& whole) {
    whole.clear();
    const size_t kMax = 256U * 1024U;
    for (;;) {
        if (whole.size() >= kMax) {
            return true;
        }
        char t[4 * 1024];
        const ssize_t n = ::read(client_fd_, t, sizeof t);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return !whole.empty();
        }
        whole.append(t, t + n);
        if (whole.find("\r\n\r\n") != std::string::npos) {
            return true;
        }
    }
}

void HttpClientSession::process_parsed(const std::string& raw) {
    const size_t p = raw.find("\r\n");
    if (p == std::string::npos) {
        send_text(400, "Bad request");
        return;
    }
    const std::string first = raw.substr(0, p);
    const size_t dbl = raw.find("\r\n\r\n", p);
    if (dbl == std::string::npos) {
        send_text(400, "No header terminator");
        return;
    }
    const std::string head_block = raw.substr(p + 2, dbl - p - 2);

    FetchRequest fr;
    std::string err;
    if (!UpstreamConnector::parse_http_proxy_target(first, head_block, fr, err)) {
        if (err.find("https") != std::string::npos) {
            send_text(501, "HTTPS/CONNECT: not implemented in this build (http only on port 80).");
        } else
            send_text(400, err);
        return;
    }

    if (fr.method == "CONNECT") {
        send_text(501, "CONNECT: not implemented.");
        return;
    }
    if (fr.method != "GET" && fr.method != "HEAD") {
        send_text(501, "Only GET and HEAD are supported in this lab build.");
        return;
    }

    const std::string orig_method = fr.method;
    const std::string url = make_full_url(fr);
    if (!filter_.is_allowed(url)) {
        if (log_) log_->block(url);
        send_text(403, "Forbidden (URL policy)");
        return;
    }

    const std::int64_t maxB = ProxyConfig::instance().cacheMaxEntryBytes();
    const std::string cache_key = "GET " + url;

    if (fr.method == "GET") {
        if (const auto* h = cache_.lookup(cache_key)) {
            if (log_) log_->cache_hit(cache_key);
            std::ostringstream o;
            o << "HTTP/1.1 " << h->status_code << ' ' << reason_phrase(h->status_code) << "\r\n";
            for (const auto& kv : h->headers) {
                o << kv.first << ": " << kv.second << "\r\n";
            }
            o << "Content-Length: " << h->body.size() << "\r\n";
            o << "Connection: close\r\n";
            o << "X-Cache: HIT\r\n";
            o << "\r\n" << h->body;
            send_buffer(o.str());
            return;
        }
        if (log_) log_->cache_miss(cache_key);
    }

    const FetchRequest upstream_req = fr;
    FetchResult res = UpstreamConnector::fetch(upstream_req, maxB);
    if (!res.ok) {
        const std::string t = (res.err.empty() ? "upstream" : res.err);
        if (log_) log_->info("fetch failed: " + t);
        std::string msg = "Failed to contact origin. ";
        msg += t;
        send_text(502, msg);
        return;
    }

    std::string body = res.body;
    if (orig_method == "HEAD") {
        body.clear();
    }

    std::ostringstream o;
    o << "HTTP/1.1 " << res.status << ' ' << reason_phrase(res.status) << "\r\n";
    for (const auto& kv : res.response_headers_lower) {
        if (kv.first == "connection" || kv.first == "transfer-encoding") {
            continue;
        }
        o << kv.first << ": " << kv.second << "\r\n";
    }
    o << "Content-Length: " << body.size() << "\r\n";
    o << "Connection: close\r\n";
    o << "X-Cache: MISS\r\n";
    o << "\r\n";
    send_buffer(o.str());
    if (!body.empty()) {
        send_buffer(body);
    }

    if (orig_method == "GET" && res.status == 200 && res.ok) {
        bool ok_cache = true;
        if (const auto it = res.response_headers_lower.find("cache-control"); it != res.response_headers_lower.end()) {
            if (it->second.find("no-store") != std::string::npos) {
                ok_cache = false;
            }
        }
        if (const auto it2 = res.response_headers_lower.find("pragma");
            it2 != res.response_headers_lower.end()) {
            if (it2->second.find("no-cache") != std::string::npos) {
                ok_cache = false;
            }
        }
        if (static_cast<std::int64_t>(res.body.size()) > maxB) {
            ok_cache = false;
        }
        if (ok_cache) {
            CacheEntry e;
            e.status_code = 200;
            e.body = res.body;
            if (const auto ct = res.response_headers_lower.find("content-type");
                ct != res.response_headers_lower.end()) {
                e.headers["Content-Type"] = ct->second;
            }
            cache_.store(cache_key, std::move(e));
        }
    }
}

void HttpClientSession::run() {
    std::string raw;
    if (!read_http_message(raw) || raw.empty()) {
        (void)close(client_fd_);
        return;
    }
    try {
        if (raw.find("\r\n\r\n") == std::string::npos) {
            send_text(400, "Incomplete request headers");
        } else
            process_parsed(raw);
    } catch (const std::exception& e) {
        (void)e;
    } catch (...) {
    }
    (void)close(client_fd_);
}

}  // namespace hp
