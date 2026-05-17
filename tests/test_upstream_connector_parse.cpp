#include "upstream_connector.h"

#include <gtest/gtest.h>

namespace {

using hp::FetchRequest;
using hp::ParsedHttpUrl;
using hp::UpstreamConnector;

TEST(ParseUrlAbsoluteTest1, BasicHostDefaultPortSlashPath) {
  ParsedHttpUrl out;
  std::string err;
  ASSERT_TRUE(UpstreamConnector::parse_url_absolute("http://example.com/foo",
                                                    out, err));
  EXPECT_EQ(out.host, "example.com");
  EXPECT_EQ(out.port, 80);
  EXPECT_EQ(out.path_and_query, "/foo");
}

TEST(ParseUrlAbsoluteTest2, RootWhenNoSlash) {
  ParsedHttpUrl out;
  std::string err;
  ASSERT_TRUE(
      UpstreamConnector::parse_url_absolute("http://example.com", out, err));
  EXPECT_EQ(out.path_and_query, "/");
}

TEST(ParseUrlAbsoluteTest3, ExplicitPortKeepsTrailingPath) {
  ParsedHttpUrl out;
  std::string err;
  ASSERT_TRUE(UpstreamConnector::parse_url_absolute(
      "http://example.com:8080/q?x=1", out, err));
  EXPECT_EQ(out.host, "example.com");
  EXPECT_EQ(out.port, 8080);
  EXPECT_EQ(out.path_and_query, "/q?x=1");
}

TEST(ParseUrlAbsoluteTest4, RejectsHttpsWithExplainErr) {
  ParsedHttpUrl out;
  std::string err;
  EXPECT_FALSE(
      UpstreamConnector::parse_url_absolute("https://secure/", out, err));
  EXPECT_FALSE(err.empty());
}

TEST(ParseUrlAbsoluteTest5, RejectsBadScheme) {
  ParsedHttpUrl out;
  std::string err;
  EXPECT_FALSE(UpstreamConnector::parse_url_absolute("ftp://x/", out, err));
}

TEST(ParseUrlAbsoluteTest6, EmptyHostFails) {
  ParsedHttpUrl out;
  std::string err;
  EXPECT_FALSE(UpstreamConnector::parse_url_absolute("http:///path", out, err));
}

TEST(ParseUrlAbsoluteTest7, BogusLargePortFallsBackTo80) {
  ParsedHttpUrl out;
  std::string err;
  ASSERT_TRUE(UpstreamConnector::parse_url_absolute(
      "http://h.example:999999/nope", out, err));
  EXPECT_EQ(out.port, 80);
}

TEST(ParseHttpProxyTargetTest1, AbsoluteUriMinimalHeaders) {
  FetchRequest fr;
  std::string err;
  ASSERT_TRUE(UpstreamConnector::parse_http_proxy_target(
      "GET http://origin/path HTTP/1.1",
      "Host: origin\r\n"
      "Accept: */*\r\n",
      fr, err));
  EXPECT_EQ(fr.method, "GET");
  EXPECT_EQ(fr.target.host, "origin");
  EXPECT_EQ(fr.target.path_and_query, "/path");
}

TEST(ParseHttpProxyTargetTest2, RelativeUriRequiresHostHeader) {
  FetchRequest fr;
  std::string err;
  ASSERT_TRUE(
      UpstreamConnector::parse_http_proxy_target("HEAD /relative?q=1 HTTP/1.1",
                                                 "Host: srv.example\r\n"
                                                 "User-Agent: ut\r\n",
                                                 fr, err));
  EXPECT_EQ(fr.method, "HEAD");
  EXPECT_EQ(fr.target.host, "srv.example");
  EXPECT_EQ(fr.target.port, 80);
  EXPECT_EQ(fr.target.path_and_query, "/relative?q=1");
}

TEST(ParseHttpProxyTargetTest3, MissingHostForRelativeFails) {
  FetchRequest fr;
  std::string err;
  EXPECT_FALSE(UpstreamConnector::parse_http_proxy_target(
      "GET /only/path HTTP/1.1", "", fr, err));
}

TEST(ParseHttpProxyTargetTest4, MalformedFirstLineFails) {
  FetchRequest fr;
  std::string err;
  EXPECT_FALSE(
      UpstreamConnector::parse_http_proxy_target("broken", "", fr, err));
}

TEST(ParseHttpProxyTargetTest5, RelativeWithExplicitHostPortHeader) {
  FetchRequest fr;
  std::string err;
  ASSERT_TRUE(UpstreamConnector::parse_http_proxy_target(
      "GET / HTTP/1.1", "Host: backend:443\r\n", fr, err));
  EXPECT_EQ(fr.target.host, "backend");
  EXPECT_EQ(fr.target.port, 443);
}

TEST(ParseHttpProxyTargetTest6, CopiesAcceptLanguageWhenPresent) {
  FetchRequest fr;
  std::string err;
  ASSERT_TRUE(
      UpstreamConnector::parse_http_proxy_target("GET http://x/y HTTP/1.1",
                                                 "Host: x\r\n"
                                                 "Accept-Language: en-us\r\n",
                                                 fr, err));
  bool saw = false;
  for (const auto &kv : fr.client_headers) {
    if (kv.first == "accept-language" && kv.second == "en-us")
      saw = true;
  }
  EXPECT_TRUE(saw);
}

TEST(ParseHttpProxyTargetTest7, IgnoresGarbageHeaderLinesGracefully) {
  FetchRequest fr;
  std::string err;
  ASSERT_TRUE(
      UpstreamConnector::parse_http_proxy_target("GET http://z/z HTTP/1.1",
                                                 "badheaderwithoutcolon\r\n"
                                                 "Host: z\r\n",
                                                 fr, err));
  EXPECT_EQ(fr.target.host, "z");
}

} // namespace
