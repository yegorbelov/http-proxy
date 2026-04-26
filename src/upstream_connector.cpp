#include "upstream_connector.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace hp {

namespace {

std::string trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\r' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\t')) s.remove_suffix(1);
    return std::string(s);
}

int connect_tcp(const std::string& host, int port, std::string& err) {
    struct addrinfo hints {};
    struct addrinfo* res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    const std::string p = std::to_string(port);
    int g = getaddrinfo(host.c_str(), p.c_str(), &hints, &res);
    if (g != 0) {
        err = gai_strerror(g);
        return -1;
    }
    int fd = -1;
    for (struct addrinfo* it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) err = "connect failed to " + host;
    return fd;
}

static bool read_n(int fd, void* p, const std::size_t n) {
    auto* b = static_cast<unsigned char*>(p);
    std::size_t t = 0;
    while (t < n) {
        const ssize_t r = ::read(fd, b + t, n - t);
        if (r <= 0) return false;
        t += static_cast<std::size_t>(r);
    }
    return true;
}

void map_header_lines(const std::string& line_block, std::map<std::string, std::string>& o) {
    o.clear();
    std::istringstream s(line_block);
    std::string l;
    while (std::getline(s, l, '\n')) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.empty()) continue;
        const auto p = l.find(':');
        if (p == std::string::npos) continue;
        std::string k = l.substr(0, p);
        for (char& c : k) c = static_cast<char>(::tolower(c));
        o[std::move(k)] = trim(l.substr(p + 1));
    }
}

int status_from_header_block(const std::string& block) {
    std::istringstream s(block);
    std::string line;
    if (!std::getline(s, line, '\n')) return 0;
    if (line.rfind("HTTP/", 0) != 0) return 0;
    std::istringstream t(line);
    std::string ver, code, rest;
    t >> ver >> code;
    (void)rest;
    return std::atoi(code.c_str());
}

static bool read_response_headers(int fd, std::string& buf) {
    buf.clear();
    const std::size_t kMax = 256u * 1024u;
    for (;;) {
        if (buf.size() > kMax) return false;
        if (buf.find("\r\n\r\n") != std::string::npos) return true;
        char t[1];
        const ssize_t n = ::read(fd, t, 1);
        if (n <= 0) return false;
        buf += static_cast<char>(t[0]);
    }
}

static std::string read_body_content_length(int fd, std::string& buf_after_headers, const std::int64_t cl,
                                            const std::int64_t max) {
    std::string b = std::move(buf_after_headers);
    if (cl < 0) {
        b.clear();
        return b;
    }
    if (b.size() > static_cast<std::size_t>(cl)) b = b.substr(0, static_cast<std::size_t>(cl));
    if (b.size() > static_cast<std::size_t>(max)) {
        b.clear();
        return b;
    }
    if (cl > 0) {
        const std::int64_t need = cl - static_cast<std::int64_t>(b.size());
        if (need < 0) {
            b = b.substr(0, static_cast<std::size_t>(cl));
            return b;
        }
        if (need + static_cast<std::int64_t>(b.size()) > max) {
            b.clear();
            return b;
        }
        const std::size_t add = static_cast<std::size_t>(need);
        b.resize(b.size() + add);
        if (add) {
            if (!read_n(fd, b.data() + (b.size() - add), add)) {
                b.clear();
            }
        }
    }
    return b;
}

static int append_sock(int fd, std::string& s) {
    char b2[8 * 1024];
    const ssize_t n = ::read(fd, b2, sizeof b2);
    if (n > 0) s.append(b2, b2 + n);
    return n > 0 ? 0 : (n < 0 ? -1 : 1);
}

static std::string read_body_chunked(int fd, std::string& rest, const std::int64_t max) {
    std::string out;
    for (;;) {
        if (out.size() > static_cast<std::size_t>(max) + 1) return out;
        while (rest.find("\r\n") == std::string::npos) {
            if (rest.size() > 16U * 1024U * 1024U) return out;
            if (const int t = append_sock(fd, rest); t != 0) return (t < 0 && out.empty() ? out : out);
        }
        const size_t p = rest.find("\r\n");
        if (p == std::string::npos) return out;
        std::string line = rest.substr(0, p);
        rest.erase(0, p + 2U);
        {
            const size_t sc = line.find(';');
            if (sc != std::string::npos) line = line.substr(0, sc);
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(0, 1);
        }
        char* endp = nullptr;
        const long sz = std::strtol(line.c_str(), &endp, 16);
        (void)endp;
        if (sz < 0) return out;
        if (sz == 0) return out;
        if (out.size() + sz > max) return out;
        const auto need = static_cast<std::int64_t>(sz);
        while (static_cast<std::int64_t>(rest.size()) < need) {
            if (const int t = append_sock(fd, rest); t != 0) return out;
        }
        if (static_cast<std::int64_t>(rest.size()) < need) return out;
        out += rest.substr(0, static_cast<std::size_t>(need));
        rest.erase(0, static_cast<std::size_t>(need));
        while (static_cast<std::int64_t>(rest.size()) < 2) {
            char t1[1];
            const ssize_t t = ::read(fd, t1, 1);
            if (t <= 0) return out;
            rest += static_cast<char>(t1[0]);
        }
        if (rest.size() >= 2U && rest[0] == '\r' && rest[1] == '\n') rest.erase(0, 2U);
    }
}

static std::string read_body_until_close(int fd, std::string& pr, const std::int64_t max) {
    for (;;) {
        if (pr.size() > static_cast<std::size_t>(max) + 1) {
            pr = pr.substr(0, static_cast<std::size_t>(max));
            return pr;
        }
        char b2[8 * 1024];
        const ssize_t n = ::read(fd, b2, sizeof b2);
        if (n < 0) {
            if (pr.size() > static_cast<std::size_t>(max)) pr = pr.substr(0, static_cast<std::size_t>(max));
            return pr;
        }
        if (n == 0) {
            if (pr.size() > static_cast<std::size_t>(max)) pr = pr.substr(0, static_cast<std::size_t>(max));
            return pr;
        }
        pr.append(b2, b2 + n);
    }
}

}  // namespace

bool UpstreamConnector::parse_url_absolute(const std::string& abs_uri, ParsedHttpUrl& out, std::string& err) {
    err.clear();
    if (abs_uri.rfind("http://", 0) != 0) {
        if (abs_uri.rfind("https://", 0) == 0) {
            err = "https is not supported for plain GET; use port 80 http";
            return false;
        }
        err = "bad scheme";
        return false;
    }
    std::string u = abs_uri.substr(7);
    const size_t pslash = u.find('/');
    std::string hostport;
    if (pslash == std::string::npos) {
        hostport = u;
        out.path_and_query = "/";
    } else {
        hostport = u.substr(0, pslash);
        out.path_and_query = u.substr(pslash);
    }
    if (out.path_and_query.empty()) out.path_and_query = "/";
    const size_t c = hostport.rfind(':');
    if (c == std::string::npos) {
        out.host = hostport;
        out.port = 80;
    } else {
        out.host = hostport.substr(0, c);
        if (c + 1 < hostport.size()) {
            int pr = std::atoi(hostport.c_str() + static_cast<int>(c) + 1);
            if (pr <= 0 || pr > 65535) pr = 80;
            out.port = pr;
        } else
            out.port = 80;
    }
    if (out.host.empty()) {
        err = "empty host";
        return false;
    }
    return true;
}

bool UpstreamConnector::parse_http_proxy_target(const std::string& first_line, const std::string& request_headers,
                                                FetchRequest& out, std::string& error) {
    error.clear();
    out.client_headers.clear();
    std::istringstream f(first_line);
    f >> out.method;
    std::string uri, ver;
    f >> uri;
    f >> ver;
    (void)ver;
    if (out.method.empty() || uri.empty()) {
        error = "malformed request line";
        return false;
    }
    std::map<std::string, std::string> h;
    std::istringstream sh(request_headers);
    std::string l;
    while (std::getline(sh, l, '\n')) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.empty() || l == " ") continue;
        const auto p = l.find(':');
        if (p == std::string::npos) continue;
        std::string k = l.substr(0, p);
        for (char& c : k) c = static_cast<char>(::tolower(c));
        h[std::move(k)] = trim(l.substr(p + 1));
    }
    if (uri.rfind("http://", 0) == 0) {
        if (!parse_url_absolute(uri, out.target, error)) return false;
    } else if (!uri.empty() && uri[0] == '/') {
        if (h.find("host") == h.end() || h["host"].empty()) {
            error = "missing Host for relative request";
            return false;
        }
        const std::string& shost = h["host"];
        const size_t c = shost.rfind(':');
        if (c == std::string::npos) {
            out.target.host = shost;
            out.target.port = 80;
        } else {
            out.target.host = shost.substr(0, c);
            if (c + 1 < shost.size()) {
                int pr = std::atoi(shost.c_str() + static_cast<int>(c) + 1);
                if (pr <= 0 || pr > 65535) pr = 80;
                out.target.port = pr;
            } else
                out.target.port = 80;
        }
        if (out.target.host.empty()) {
            error = "empty host";
            return false;
        }
        out.target.path_and_query = uri;
    } else {
        error = "invalid request URI in proxy form";
        return false;
    }
    if (h.find("host") != h.end()) out.client_headers.push_back({"host", h["host"]});
    for (const char* k2 : {"user-agent", "accept", "accept-language"}) {
        if (h.find(k2) != h.end()) out.client_headers.push_back({k2, h[k2]});
    }
    return true;
}

static std::string build_upstream_request(const FetchRequest& r) {
    std::ostringstream o;
    o << r.method << " " << r.target.path_and_query << " HTTP/1.1\r\n";
    o << "Host: " << r.target.host;
    if (r.target.port != 80) o << ":" << r.target.port;
    o << "\r\n";
    o << "Connection: close\r\n";
    for (const auto& p : r.client_headers) {
        if (p.first == "host") continue;
        if (p.first == "content-length" || p.first == "transfer-encoding") continue;
        o << p.first << ": " << p.second << "\r\n";
    }
    o << "Accept: */*\r\n";
    if (!r.request_body.empty()) o << "Content-Length: " << r.request_body.size() << "\r\n";
    o << "\r\n";
    std::string s = o.str();
    s += r.request_body;
    return s;
}

FetchResult UpstreamConnector::fetch(const FetchRequest& req, const std::int64_t max_body) {
    FetchResult r;
    std::string werr;
    int fd = connect_tcp(req.target.host, req.target.port, werr);
    if (fd < 0) {
        r.ok = false;
        r.err = werr;
        return r;
    }
    const std::string sreq = build_upstream_request(req);
    {
        const char* p = sreq.c_str();
        std::size_t L = sreq.size();
        while (L) {
            const ssize_t t = ::write(fd, p, L);
            if (t < 0) {
                r.err = "write";
                r.ok = false;
                close(fd);
                return r;
            }
            p += static_cast<std::size_t>(t);
            L -= static_cast<std::size_t>(t);
        }
    }
    std::string raw;
    if (!read_response_headers(fd, raw)) {
        r.ok = false;
        r.err = "bad response header from origin";
        close(fd);
        return r;
    }
    const size_t dbl = raw.find("\r\n\r\n");
    if (dbl == std::string::npos) {
        r.ok = false;
        r.err = "incomplete response header";
        close(fd);
        return r;
    }
    r.raw_header_block = raw.substr(0, dbl);
    r.status = status_from_header_block(r.raw_header_block);
    r.ok = (r.status > 0);
    if (!r.ok) {
        r.err = "no HTTP status in origin";
        close(fd);
        return r;
    }
    std::string honly = r.raw_header_block;
    {
        const size_t fl = honly.find("\r\n");
        if (fl != std::string::npos) honly = honly.substr(fl + 2u);
    }
    map_header_lines(honly, r.response_headers_lower);
    std::string pr = raw.substr(dbl + 4u);
    const bool is_chunked =
        r.response_headers_lower.find("transfer-encoding") != r.response_headers_lower.end() &&
        r.response_headers_lower["transfer-encoding"].find("chunked") != std::string::npos;
    if (r.response_headers_lower.find("content-length") != r.response_headers_lower.end() && !is_chunked) {
        const std::int64_t cl = std::atoll(r.response_headers_lower["content-length"].c_str());
        r.body = read_body_content_length(fd, pr, cl, max_body);
    } else if (is_chunked) {
        r.body = read_body_chunked(fd, pr, max_body);
    } else
        r.body = read_body_until_close(fd, pr, max_body);
    (void)close(fd);
    (void)max_body;
    return r;
}

}  // namespace hp
