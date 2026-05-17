#include "http_client_session.h"
#include "proxy_config.h"
#include "response_cache.h"
#include "url_filter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

std::string read_http_response_best_effort(int fd) {
  std::string resp;
  char buf[8192];
  for (;;) {
    const ssize_t n = ::read(fd, buf, sizeof buf);
    if (n <= 0)
      break;
    resp.append(buf, buf + n);
    if (resp.find("\r\n\r\n") != std::string::npos)
      break;
  }
  return resp;
}

TEST(HttpClientSessionSocketTest1, ForbiddenWhenDeniedByPolicy) {
  hp::ProxyConfig::instance().load("/nonexistent/use_defaults.ini");

  int fds[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

  hp::UrlFilter filter({}, {"blocked.bad"});
  hp::ResponseCache cache;

  std::thread worker([&]() {
    hp::HttpClientSession sess(fds[0], filter, cache, nullptr);
    sess.run();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  const std::string req = "GET http://blocked.bad/path HTTP/1.1\r\n"
                          "Host: blocked.bad\r\n\r\n";
  ASSERT_GT(::write(fds[1], req.data(), req.size()), 0);

  const std::string resp = read_http_response_best_effort(fds[1]);
  ::close(fds[1]);
  worker.join();

  EXPECT_NE(resp.find("403"), std::string::npos);
  EXPECT_NE(resp.find("Forbidden"), std::string::npos);
}

TEST(HttpClientSessionSocketTest2, BadRequestWhenMalformedFirstLine) {
  hp::ProxyConfig::instance().load("/nonexistent/use_defaults.ini");

  int fds[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

  hp::UrlFilter filter({}, {});
  hp::ResponseCache cache;

  std::thread worker([&]() {
    hp::HttpClientSession sess(fds[0], filter, cache, nullptr);
    sess.run();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  const std::string req = "NOT_HTTP_LINE\r\n\r\n";
  ASSERT_GT(::write(fds[1], req.data(), req.size()), 0);

  const std::string resp = read_http_response_best_effort(fds[1]);
  ::close(fds[1]);
  worker.join();

  EXPECT_NE(resp.find("400"), std::string::npos);
}

} // namespace
