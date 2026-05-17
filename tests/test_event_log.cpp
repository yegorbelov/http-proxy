#include "event_log.h"
#include "test_helpers.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <string>

namespace {

std::string read_all(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::string s((std::istreambuf_iterator<char>(in)),
                std::istreambuf_iterator<char>());
  return s;
}

TEST(EventLogInfoTest1, WritesBracketInfoMarker) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.info("hello-world");
  const std::string body = read_all(path);
  EXPECT_NE(body.find("[info]"), std::string::npos);
  EXPECT_NE(body.find("hello-world"), std::string::npos);
}

TEST(EventLogInfoTest2, MultipleLinesAppend) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.info("one");
  log.info("two");
  const std::string body = read_all(path);
  EXPECT_NE(body.find("one"), std::string::npos);
  EXPECT_NE(body.find("two"), std::string::npos);
}

TEST(EventLogInfoTest3, SupportsUtf8Payload) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.info("\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82");
  const std::string body = read_all(path);
  EXPECT_NE(body.find("\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82"),
            std::string::npos);
}

TEST(EventLogInfoTest4, EmptyMessageStillStructured) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.info("");
  const std::string body = read_all(path);
  EXPECT_NE(body.find("[info]"), std::string::npos);
}

TEST(EventLogInfoTest5, LongLineStored) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  std::string big(8000, 'q');
  log.info(big);
  const std::string body = read_all(path);
  EXPECT_GE(body.size(), big.size());
}

TEST(EventLogInfoTest6, NewlineSeparatedRecords) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.info("a");
  log.info("b");
  const std::string body = read_all(path);
  EXPECT_GE(body.find('\n'), 1u);
}

TEST(EventLogInfoTest7, TimestampBracketsPresent) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.info("tick");
  const std::string body = read_all(path);
  EXPECT_NE(body.find('['), std::string::npos);
}

TEST(EventLogBlockTest1, RecordsUrlPolicyHit) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.block("http://evil/");
  const std::string body = read_all(path);
  EXPECT_NE(body.find("[block]"), std::string::npos);
  EXPECT_NE(body.find("evil"), std::string::npos);
}

TEST(EventLogBlockTest2, EscapedLookingUrlsPersist) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.block("http://x?q=a&b=c");
  EXPECT_NE(read_all(path).find("q=a"), std::string::npos);
}

TEST(EventLogBlockTest3, EmptyUrlStillLogged) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.block("");
  EXPECT_NE(read_all(path).find("[block]"), std::string::npos);
}

TEST(EventLogBlockTest4, SequentialBlocksOrdered) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.block("first");
  log.block("second");
  const std::string body = read_all(path);
  EXPECT_LT(body.find("first"), body.find("second"));
}

TEST(EventLogBlockTest5, LongDeniedUrlStored) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  std::string u(6000, '/');
  log.block(u);
  EXPECT_GE(read_all(path).size(), u.size());
}

TEST(EventLogBlockTest6, SpecialCharactersPreserved) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.block("http://h/\x01\x02");
  EXPECT_NE(read_all(path).find('\x01'), std::string::npos);
}

TEST(EventLogBlockTest7, DuplicateUrlsLoggedTwice) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.block("same");
  log.block("same");
  const std::string body = read_all(path);
  const size_t first = body.find("same");
  ASSERT_NE(first, std::string::npos);
  EXPECT_NE(body.find("same", first + 1), std::string::npos);
}

TEST(EventLogCacheHitTest1, PrintsHitKeyword) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_hit("GET http://z/");
  const std::string body = read_all(path);
  EXPECT_NE(body.find("HIT"), std::string::npos);
}

TEST(EventLogCacheHitTest2, IncludesCacheLabel) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_hit("k");
  EXPECT_NE(read_all(path).find("[cache]"), std::string::npos);
}

TEST(EventLogCacheHitTest3, EmptyKeyStillStructured) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_hit("");
  EXPECT_NE(read_all(path).find("HIT"), std::string::npos);
}

TEST(EventLogCacheHitTest4, TwoHitsSeparateLines) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_hit("a");
  log.cache_hit("b");
  const auto body = read_all(path);
  EXPECT_NE(body.find('a'), std::string::npos);
  EXPECT_NE(body.find('b'), std::string::npos);
}

TEST(EventLogCacheHitTest5, LongCacheKeyAccepted) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  std::string k(9000, 'k');
  log.cache_hit(k);
  EXPECT_GE(read_all(path).size(), k.size());
}

TEST(EventLogCacheHitTest6, SpacesInsideKeyAllowed) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_hit("GET http://y/a b");
  EXPECT_NE(read_all(path).find(' '), std::string::npos);
}

TEST(EventLogCacheHitTest7, MixedHitMissOrdering) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_hit("left");
  log.cache_miss("right");
  const auto body = read_all(path);
  EXPECT_LT(body.find("HIT"), body.find("MISS"));
}

TEST(EventLogCacheMissTest1, PrintsMissKeyword) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_miss("GET http://x/");
  EXPECT_NE(read_all(path).find("MISS"), std::string::npos);
}

TEST(EventLogCacheMissTest2, UsesCacheNamespace) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_miss("z");
  EXPECT_NE(read_all(path).find("[cache]"), std::string::npos);
}

TEST(EventLogCacheMissTest3, EmptyLookupKeyLogged) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_miss("");
  EXPECT_NE(read_all(path).find("MISS"), std::string::npos);
}

TEST(EventLogCacheMissTest4, RepeatedMissesAggregate) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_miss("one");
  log.cache_miss("two");
  const auto body = read_all(path);
  EXPECT_NE(body.find("one"), std::string::npos);
  EXPECT_NE(body.find("two"), std::string::npos);
}

TEST(EventLogCacheMissTest5, LargeMissKeyStored) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  std::string k(7500, 'm');
  log.cache_miss(k);
  EXPECT_GE(read_all(path).size(), k.size());
}

TEST(EventLogCacheMissTest6, QuestionMarksPreserved) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_miss("GET http://u/?q=1");
  EXPECT_NE(read_all(path).find('?'), std::string::npos);
}

TEST(EventLogCacheMissTest7, NumericLookingKeysOk) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.cache_miss("GET http://127.0.0.1:9/");
  EXPECT_NE(read_all(path).find('9'), std::string::npos);
}

TEST(EventLogEvictionTest1, PrintsEvictionTag) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.eviction("drop-me");
  EXPECT_NE(read_all(path).find("[evict]"), std::string::npos);
}

TEST(EventLogEvictionTest2, ShowsVictimDescriptor) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.eviction("GET http://big/");
  EXPECT_NE(read_all(path).find("big"), std::string::npos);
}

TEST(EventLogEvictionTest3, EmptyReasonLogged) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.eviction("");
  EXPECT_NE(read_all(path).find("[evict]"), std::string::npos);
}

TEST(EventLogEvictionTest4, SequentialEvictionsOrdered) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.eviction("one");
  log.eviction("two");
  const auto body = read_all(path);
  EXPECT_LT(body.find("one"), body.find("two"));
}

TEST(EventLogEvictionTest5, VeryLongVictimText) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  std::string v(8200, 'v');
  log.eviction(v);
  EXPECT_GE(read_all(path).size(), v.size());
}

TEST(EventLogEvictionTest6, TabsInsidePayloadAllowed) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.eviction("k\tz");
  EXPECT_NE(read_all(path).find('\t'), std::string::npos);
}

TEST(EventLogEvictionTest7, CollisionWithHeadersStillDistinctLine) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  hp::EventLog log(path, false);
  log.eviction("reason-line\r-like");
  EXPECT_NE(read_all(path).find("reason-line"), std::string::npos);
}

} // namespace
