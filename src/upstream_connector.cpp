#include "upstream_connector.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>

namespace hp {

std::string trim(const std::string &s) {
  size_t start = 0;
  size_t end = s.size();
  while (start < end &&
         (s[start] == ' ' || s[start] == '\r' || s[start] == '\t'))
    start++;
  while (end > start &&
         (s[end - 1] == ' ' || s[end - 1] == '\r' || s[end - 1] == '\t'))
    end--;
  return s.substr(start, end - start);
}

int connect_tcp(const std::string &host, int port, std::string &err) {
  struct addrinfo hints{};
  struct addrinfo *res = nullptr;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  const std::string p = std::to_string(port);
  int g = getaddrinfo(host.c_str(), p.c_str(), &hints, &res);
  if (g != 0) {
    err = gai_strerror(g);
    return -1;
  }
  int fd = -1;
  for (struct addrinfo *it = res; it; it = it->ai_next) {
    fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (fd < 0)
      continue;
    if (connect(fd, it->ai_addr, it->ai_addrlen) == 0)
      break;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0)
    err = "connect failed to " + host;
  return fd;
}

void map_header_lines(const std::string &line_block,
                      std::map<std::string, std::string> &out) {
  out.clear();
  std::istringstream ss(line_block);
  std::string line;
  while (std::getline(ss, line, '\n')) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      continue;
    size_t colon = line.find(':');
    if (colon == std::string::npos)
      continue;
    std::string key = line.substr(0, colon);
    for (char &c : key)
      c = tolower(c);
    out[key] = trim(line.substr(colon + 1));
  }
}

int status_from_header_block(const std::string &block) {
  std::istringstream ss(block);
  std::string line;
  if (!std::getline(ss, line, '\n'))
    return 0;
  if (line.rfind("HTTP/", 0) != 0)
    return 0;
  std::istringstream t(line);
  std::string ver, code;
  t >> ver >> code;
  return std::atoi(code.c_str());
}

bool read_response_headers(int fd, std::string &buf) {
  buf.clear();
  char c;
  while (buf.size() < 256 * 1024) {
    if (::read(fd, &c, 1) <= 0)
      return false;
    buf += c;
    if (buf.size() >= 4 && buf.compare(buf.size() - 4, 4, "\r\n\r\n") == 0)
      return true;
  }
  return false;
}

std::string read_body_content_length(int fd, const std::string &already_read,
                                     long long cl, long long max) {
  if (cl <= 0 || cl > max)
    return "";
  std::string result = already_read.substr(0, (size_t)cl);
  while ((long long)result.size() < cl) {
    char buf[4096];
    long long needed = cl - (long long)result.size();
    ssize_t n = ::read(fd, buf,
                       (size_t)(needed < (long long)sizeof(buf)
                                    ? needed
                                    : (long long)sizeof(buf)));
    if (n <= 0)
      break;
    result.append(buf, n);
  }
  return result;
}

std::string read_body_chunked(int fd, std::string &rest, long long max) {
  std::string result;
  for (;;) {
    while (rest.find("\r\n") == std::string::npos) {
      char buf[4096];
      ssize_t n = ::read(fd, buf, sizeof(buf));
      if (n <= 0)
        return result;
      rest.append(buf, n);
    }
    size_t pos = rest.find("\r\n");
    std::string size_str = rest.substr(0, pos);
    rest.erase(0, pos + 2);
    long chunk_size = std::strtol(size_str.c_str(), nullptr, 16);
    if (chunk_size <= 0)
      break;
    if ((long long)(result.size() + chunk_size) > max)
      break;
    while ((long long)rest.size() < chunk_size) {
      char buf[4096];
      ssize_t n = ::read(fd, buf, sizeof(buf));
      if (n <= 0)
        return result;
      rest.append(buf, n);
    }
    result += rest.substr(0, (size_t)chunk_size);
    rest.erase(0, (size_t)chunk_size);
    while (rest.size() < 2) {
      char c;
      if (::read(fd, &c, 1) <= 0)
        return result;
      rest += c;
    }
    if (rest[0] == '\r' && rest[1] == '\n')
      rest.erase(0, 2);
  }
  return result;
}

std::string read_body_until_close(int fd, const std::string &already_read,
                                  long long max) {
  std::string result = already_read;
  char buf[8192];
  while ((long long)result.size() < max) {
    ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n <= 0)
      break;
    result.append(buf, n);
  }
  if ((long long)result.size() > max)
    result.resize((size_t)max);
  return result;
}

bool UpstreamConnector::parse_url_absolute(const std::string &abs_uri,
                                           ParsedHttpUrl &out,
                                           std::string &err) {
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
  if (out.path_and_query.empty())
    out.path_and_query = "/";
  const size_t colon = hostport.rfind(':');
  if (colon == std::string::npos) {
    out.host = hostport;
    out.port = 80;
  } else {
    out.host = hostport.substr(0, colon);
    if (colon + 1 < hostport.size()) {
      int pr = std::atoi(hostport.c_str() + (int)colon + 1);
      if (pr <= 0 || pr > 65535)
        pr = 80;
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

bool UpstreamConnector::parse_http_proxy_target(
    const std::string &first_line, const std::string &request_headers,
    FetchRequest &out, std::string &error) {
  error.clear();
  out.client_headers.clear();
  std::istringstream f(first_line);
  f >> out.method;
  std::string uri, ver;
  f >> uri;
  f >> ver;
  if (out.method.empty() || uri.empty()) {
    error = "malformed request line";
    return false;
  }
  std::map<std::string, std::string> h;
  std::istringstream sh(request_headers);
  std::string l;
  while (std::getline(sh, l, '\n')) {
    if (!l.empty() && l.back() == '\r')
      l.pop_back();
    if (l.empty() || l == " ")
      continue;
    size_t colon = l.find(':');
    if (colon == std::string::npos)
      continue;
    std::string key = l.substr(0, colon);
    for (char &c : key)
      c = tolower(c);
    h[key] = trim(l.substr(colon + 1));
  }
  if (uri.rfind("http://", 0) == 0) {
    if (!parse_url_absolute(uri, out.target, error))
      return false;
  } else if (!uri.empty() && uri[0] == '/') {
    if (h.find("host") == h.end() || h["host"].empty()) {
      error = "missing Host for relative request";
      return false;
    }
    const std::string &shost = h["host"];
    const size_t colon = shost.rfind(':');
    if (colon == std::string::npos) {
      out.target.host = shost;
      out.target.port = 80;
    } else {
      out.target.host = shost.substr(0, colon);
      if (colon + 1 < shost.size()) {
        int pr = std::atoi(shost.c_str() + (int)colon + 1);
        if (pr <= 0 || pr > 65535)
          pr = 80;
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
  if (h.find("host") != h.end())
    out.client_headers.push_back({"host", h["host"]});
  for (const char *k : {"user-agent", "accept", "accept-language"}) {
    if (h.find(k) != h.end())
      out.client_headers.push_back({k, h[k]});
  }
  return true;
}

std::string build_upstream_request(const FetchRequest &r) {
  std::ostringstream o;
  o << r.method << " " << r.target.path_and_query << " HTTP/1.1\r\n";
  o << "Host: " << r.target.host;
  if (r.target.port != 80)
    o << ":" << r.target.port;
  o << "\r\n";
  o << "Connection: close\r\n";
  for (const auto &p : r.client_headers) {
    if (p.first == "host")
      continue;
    if (p.first == "content-length" || p.first == "transfer-encoding")
      continue;
    o << p.first << ": " << p.second << "\r\n";
  }
  o << "Accept: */*\r\n";
  if (!r.request_body.empty())
    o << "Content-Length: " << r.request_body.size() << "\r\n";
  o << "\r\n";
  return o.str() + r.request_body;
}

FetchResult UpstreamConnector::fetch(const FetchRequest &req,
                                     long long max_body) {
  FetchResult r;
  std::string werr;
  int fd = connect_tcp(req.target.host, req.target.port, werr);
  if (fd < 0) {
    r.ok = false;
    r.err = werr;
    return r;
  }
  const std::string sreq = build_upstream_request(req);
  if (::write(fd, sreq.c_str(), sreq.size()) < 0) {
    r.ok = false;
    r.err = "write failed";
    close(fd);
    return r;
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
    size_t fl = honly.find("\r\n");
    if (fl != std::string::npos)
      honly = honly.substr(fl + 2);
  }
  map_header_lines(honly, r.response_headers_lower);
  std::string rest = raw.substr(dbl + 4);
  const bool is_chunked = r.response_headers_lower.find("transfer-encoding") !=
                              r.response_headers_lower.end() &&
                          r.response_headers_lower["transfer-encoding"].find(
                              "chunked") != std::string::npos;
  if (r.response_headers_lower.find("content-length") !=
          r.response_headers_lower.end() &&
      !is_chunked) {
    const long long cl =
        std::atoll(r.response_headers_lower["content-length"].c_str());
    r.body = read_body_content_length(fd, rest, cl, max_body);
  } else if (is_chunked) {
    r.body = read_body_chunked(fd, rest, max_body);
  } else {
    r.body = read_body_until_close(fd, rest, max_body);
  }
  close(fd);
  return r;
}

} // namespace hp
