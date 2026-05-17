#include "proxy_config.h"
#include "test_helpers.hpp"

#include <gtest/gtest.h>

namespace {

using hp::ProxyConfig;

TEST(ProxyConfigLoadTest1, MissingFileKeepsDefaults) {
  ProxyConfig::instance().load(
      "/nonexistent/path/to/config_that_does_not_exist.ini");
  EXPECT_EQ(ProxyConfig::instance().listenHost(), "0.0.0.0");
  EXPECT_EQ(ProxyConfig::instance().listenPort(), 8080);
}

TEST(ProxyConfigLoadTest2, OverridesListenHostPort) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, "listen_host=127.0.0.1\n"
                               "listen_port=9090\n"));
  ProxyConfig::instance().load(path);
  EXPECT_EQ(ProxyConfig::instance().listenHost(), "127.0.0.1");
  EXPECT_EQ(ProxyConfig::instance().listenPort(), 9090);
}

TEST(ProxyConfigLoadTest3, CommentsAndBlankLinesIgnored) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, "\n"
                               "# comment\n"
                               "\n"
                               "listen_port=7777\n"));
  ProxyConfig::instance().load(path);
  EXPECT_EQ(ProxyConfig::instance().listenPort(), 7777);
}

TEST(ProxyConfigLoadTest4, AllowDenyListsSemicolonSeparated) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, "allow_url_patterns=a.com;b.com\n"
                               "deny_url_patterns=evil;x.y\n"));
  ProxyConfig::instance().load(path);
  const auto &a = ProxyConfig::instance().allowUrlPatterns();
  const auto &d = ProxyConfig::instance().denyUrlPatterns();
  ASSERT_EQ(a.size(), 2u);
  EXPECT_EQ(a[0], "a.com");
  EXPECT_EQ(a[1], "b.com");
  ASSERT_EQ(d.size(), 2u);
  EXPECT_EQ(d[0], "evil");
  EXPECT_EQ(d[1], "x.y");
}

TEST(ProxyConfigLoadTest5, AdminReloadBooleanVariants) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, "admin_reload_enabled=false\n"));
  ProxyConfig::instance().load(path);
  EXPECT_FALSE(ProxyConfig::instance().adminReloadEnabled());

  ASSERT_TRUE(write_file(path, "admin_reload_enabled=1\n"));
  ProxyConfig::instance().load(path);
  EXPECT_TRUE(ProxyConfig::instance().adminReloadEnabled());
}

TEST(ProxyConfigLoadTest6, CacheSizesReflectBytesHelpers) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, "cache_max_size_mb=2\n"
                               "cache_max_entry_size_mb=1\n"));
  ProxyConfig::instance().load(path);
  EXPECT_EQ(ProxyConfig::instance().cacheMaxBytes(), 2LL * 1024 * 1024);
  EXPECT_EQ(ProxyConfig::instance().cacheMaxEntryBytes(), 1LL * 1024 * 1024);
}

TEST(ProxyConfigLoadTest7, InvalidPortFallsBackToDefault8080) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, "listen_port=99999999\n"
                               "listen_port=abc\n"));
  ProxyConfig::instance().load(path);
  EXPECT_EQ(ProxyConfig::instance().listenPort(), 8080);
}

} // namespace
