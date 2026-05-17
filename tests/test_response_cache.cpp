#include "proxy_config.h"
#include "response_cache.h"
#include "test_helpers.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

namespace {

hp::CacheEntry make_entry(const std::string &body, long long expires = 0) {
  hp::CacheEntry e;
  e.status_code = 200;
  e.body = body;
  e.headers["Content-Type"] = "text/plain";
  e.expires_at_unix = expires;
  return e;
}

std::string ini_small_cache(int ttl_sec = 3600) {
  return std::string("cache_max_size_mb=1\n"
                     "cache_max_entry_size_mb=1\n"
                     "default_ttl_sec=") +
         std::to_string(ttl_sec) +
         "\n"
         "listen_port=8080\n";
}

TEST(ResponseCacheLookupTest1, MissWhenEmpty) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  EXPECT_EQ(cache.lookup("no"), nullptr);
}

TEST(ResponseCacheLookupTest2, HitAfterStore) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("k1", make_entry("hello"));
  const auto *p = cache.lookup("k1");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->body, "hello");
}

TEST(ResponseCacheLookupTest3, MissWrongKey) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("only", make_entry("x"));
  EXPECT_EQ(cache.lookup("other"), nullptr);
}

TEST(ResponseCacheLookupTest4, MissAfterSleepWhenShortTtlExpiresOnLookup) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache(1)));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("ttl1", make_entry("bye"));
  std::this_thread::sleep_for(std::chrono::seconds(2));
  EXPECT_EQ(cache.lookup("ttl1"), nullptr);
}

TEST(ResponseCacheStoreTest1, ReplaceSameKeyUpdatesContents) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("same", make_entry("one"));
  cache.store("same", make_entry("two"));
  ASSERT_NE(cache.lookup("same"), nullptr);
  EXPECT_EQ(cache.lookup("same")->body, "two");
}

TEST(ResponseCacheStoreTest2, OversizedBodyRejected) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  std::string huge((size_t)hp::ProxyConfig::instance().cacheMaxEntryBytes() + 1,
                   'z');
  cache.store("big", make_entry(huge));
  EXPECT_EQ(cache.lookup("big"), nullptr);
}

TEST(ResponseCacheStoreTest4, ZeroByteBodyStillTracked) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("empty_body", make_entry(""));
  ASSERT_NE(cache.lookup("empty_body"), nullptr);
}

TEST(ResponseCacheStoreTest5, SingleCharacterPayloadStores) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("onechar", make_entry("z"));
  EXPECT_EQ(cache.lookup("onechar")->body, "z");
}

TEST(ResponseCacheStoreTest6, MaximumAllowedBodyStoresExactlyAtLimit) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, "cache_max_size_mb=256\n"
                               "cache_max_entry_size_mb=1\n"
                               "default_ttl_sec=600\n"));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  const auto max_b = (size_t)hp::ProxyConfig::instance().cacheMaxEntryBytes();
  cache.store("edge", make_entry(std::string(max_b, 'e')));
  ASSERT_NE(cache.lookup("edge"), nullptr);
}

TEST(ResponseCacheStoreTest7, SequentialDistinctKeysAccumulateUntilEvicted) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("s1", make_entry(std::string(100000, '1')));
  cache.store("s2", make_entry(std::string(100000, '2')));
  ASSERT_NE(cache.lookup("s1"), nullptr);
  ASSERT_NE(cache.lookup("s2"), nullptr);
}

TEST(ResponseCacheStoreTest3, EvictsOldestWhenOverCapacityManyBodies) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  std::string chunk(350000, 'a');
  cache.store("k1", make_entry(chunk));
  cache.store("k2", make_entry(chunk));
  cache.store("k3", make_entry(chunk));
  EXPECT_EQ(cache.lookup("k1"), nullptr);
  ASSERT_NE(cache.lookup("k2"), nullptr);
  ASSERT_NE(cache.lookup("k3"), nullptr);
}

TEST(ResponseCacheInvalidateExpiredTest1, DropsExpiredEntriesViaSweep) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache(1)));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("fresh", make_entry("ok"));
  std::this_thread::sleep_for(std::chrono::seconds(2));
  cache.invalidate_expired();
  EXPECT_EQ(cache.lookup("fresh"), nullptr);
}

TEST(ResponseCacheInvalidateExpiredTest2,
     KeepsFreshEntriesWhenSweepImmediately) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("alive", make_entry("yes"));
  cache.invalidate_expired();
  ASSERT_NE(cache.lookup("alive"), nullptr);
}

TEST(ResponseCacheInvalidateExpiredTest3, EmptyCacheSweepNoThrow) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.invalidate_expired();
  EXPECT_EQ(cache.lookup("none"), nullptr);
}

TEST(ResponseCacheInvalidateExpiredTest4, RepeatedSweepStable) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("r", make_entry("v"));
  cache.invalidate_expired();
  cache.invalidate_expired();
  ASSERT_NE(cache.lookup("r"), nullptr);
}

TEST(ResponseCacheInvalidateExpiredTest5, SweepAfterLookupStillConsistent) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("q", make_entry("data"));
  ASSERT_NE(cache.lookup("q"), nullptr);
  cache.invalidate_expired();
  ASSERT_NE(cache.lookup("q"), nullptr);
}

TEST(ResponseCacheInvalidateExpiredTest6, MixedSweepKeepsFreshWithinWindow) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache(600)));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("still", make_entry("here"));
  cache.invalidate_expired();
  ASSERT_NE(cache.lookup("still"), nullptr);
}

TEST(ResponseCacheInvalidateExpiredTest7, SweepDoesNotCrashAfterEvictionStorm) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  std::string chunk(350000, 'y');
  cache.store("y1", make_entry(chunk));
  cache.store("y2", make_entry(chunk));
  cache.store("y3", make_entry(chunk));
  cache.invalidate_expired();
  EXPECT_NO_THROW(cache.invalidate_expired());
}

TEST(ResponseCacheEvictUntilFitTest1,
     ForcesEvictionUntilReservationFitsCapacity) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  std::string chunk(350000, 'b');
  cache.store("e1", make_entry(chunk));
  cache.store("e2", make_entry(chunk));
  const long long cap = hp::ProxyConfig::instance().cacheMaxBytes();
  cache.evict_until_fit(cap);
  EXPECT_EQ(cache.lookup("e1"), nullptr);
  EXPECT_EQ(cache.lookup("e2"), nullptr);
  cache.store("tiny", make_entry("x"));
  ASSERT_NE(cache.lookup("tiny"), nullptr);
}

TEST(ResponseCacheEvictUntilFitTest2, EmptyCacheEvictUntilFitNoCrash) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.evict_until_fit(1LL << 30);
  EXPECT_EQ(cache.lookup("none"), nullptr);
}

TEST(ResponseCacheEvictUntilFitTest3, ZeroNeedDoesNotEvictSmallFootprints) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("keep", make_entry("tiny"));
  cache.evict_until_fit(0);
  ASSERT_NE(cache.lookup("keep"), nullptr);
}

TEST(ResponseCacheEvictUntilFitTest4, TinyPositiveNeedKeepsSmallMapsSafe) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("z", make_entry("body"));
  cache.evict_until_fit(1);
  ASSERT_NE(cache.lookup("z"), nullptr);
}

TEST(ResponseCacheEvictUntilFitTest5, LargeReservationClearsFilledCache) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("solo", make_entry(std::string(300000, 's')));
  const long long cap = hp::ProxyConfig::instance().cacheMaxBytes();
  cache.evict_until_fit(cap * 2);
  EXPECT_EQ(cache.lookup("solo"), nullptr);
}

TEST(ResponseCacheEvictUntilFitTest6, RepeatedCallsIdempotentAfterEviction) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("go", make_entry("x"));
  const long long cap = hp::ProxyConfig::instance().cacheMaxBytes();
  cache.evict_until_fit(cap);
  cache.evict_until_fit(cap);
  EXPECT_EQ(cache.lookup("go"), nullptr);
}

TEST(ResponseCacheEvictUntilFitTest7, NegativeNeverHappensButSmallNeedHandled) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("negtest", make_entry("payload"));
  cache.evict_until_fit(1024);
  ASSERT_NE(cache.lookup("negtest"), nullptr);
}

TEST(ResponseCacheTouchOnHitTest1, EquivalentToSuccessfulLookupPath) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("hitme", make_entry("payload"));
  cache.touch_on_hit("hitme");
  const auto *p = cache.lookup("hitme");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->body, "payload");
}

TEST(ResponseCacheTouchOnHitTest2, TouchOnMissIgnoredSafe) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.touch_on_hit("ghost");
  EXPECT_EQ(cache.lookup("ghost"), nullptr);
}

TEST(ResponseCacheTouchOnHitTest3, MultipleTouchesPreservePayload) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("multi", make_entry("body"));
  cache.touch_on_hit("multi");
  cache.touch_on_hit("multi");
  ASSERT_NE(cache.lookup("multi"), nullptr);
  EXPECT_EQ(cache.lookup("multi")->body, "body");
}

TEST(ResponseCacheTouchOnHitTest4, TouchThenInvalidateKeepsFresh) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("mix", make_entry("ok"));
  cache.touch_on_hit("mix");
  cache.invalidate_expired();
  ASSERT_NE(cache.lookup("mix"), nullptr);
}

TEST(ResponseCacheTouchOnHitTest5, DistinctKeysIndependent) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("ka", make_entry("a"));
  cache.store("kb", make_entry("b"));
  cache.touch_on_hit("kb");
  ASSERT_NE(cache.lookup("ka"), nullptr);
}

TEST(ResponseCacheTouchOnHitTest6, TouchDoesNotInsertPhantomKeys) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("real", make_entry("z"));
  cache.touch_on_hit("missing");
  EXPECT_EQ(cache.lookup("missing"), nullptr);
  ASSERT_NE(cache.lookup("real"), nullptr);
}

TEST(ResponseCacheTouchOnHitTest7, TouchWorksAfterEvictionClearsOthers) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  std::string chunk(350000, 'q');
  cache.store("qa", make_entry(chunk));
  cache.store("qb", make_entry(chunk));
  cache.touch_on_hit("qa");
  cache.store("qc", make_entry(chunk));
  ASSERT_NE(cache.lookup("qc"), nullptr);
}

TEST(ResponseCacheSetPolicyTest1, ClearsPriorEntriesAndCounters) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("p", make_entry("data"));
  ASSERT_NE(cache.lookup("p"), nullptr);
  cache.set_policy(std::make_unique<hp::LruEvictionPolicy>());
  EXPECT_EQ(cache.lookup("p"), nullptr);
}

TEST(ResponseCacheSetPolicyTest2, ReplacePolicyKeepsServingAfterStore) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.set_policy(std::make_unique<hp::LruEvictionPolicy>());
  cache.store("n", make_entry("ok"));
  ASSERT_NE(cache.lookup("n"), nullptr);
}

TEST(ResponseCacheSetPolicyTest3, NullPolicyRecreatesInternalLru) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.store("gone", make_entry("data"));
  cache.set_policy(nullptr);
  EXPECT_EQ(cache.lookup("gone"), nullptr);
  cache.store("fresh", make_entry("ok"));
  ASSERT_NE(cache.lookup("fresh"), nullptr);
}

TEST(ResponseCacheSetPolicyTest4, SwapPoliciesClearsTwiceSafely) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.set_policy(std::make_unique<hp::LruEvictionPolicy>());
  cache.store("one", make_entry("a"));
  cache.set_policy(std::make_unique<hp::LruEvictionPolicy>());
  EXPECT_EQ(cache.lookup("one"), nullptr);
}

TEST(ResponseCacheSetPolicyTest5, ExplicitExternalPolicyPersistsAcrossStores) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.set_policy(std::make_unique<hp::LruEvictionPolicy>());
  cache.store("ext", make_entry("policy"));
  ASSERT_NE(cache.lookup("ext"), nullptr);
}

TEST(ResponseCacheSetPolicyTest6, PolicySwapThenEvictionStillWorks) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.set_policy(std::make_unique<hp::LruEvictionPolicy>());
  std::string chunk(350000, 'w');
  cache.store("w1", make_entry(chunk));
  cache.store("w2", make_entry(chunk));
  cache.store("w3", make_entry(chunk));
  ASSERT_NE(cache.lookup("w3"), nullptr);
}

TEST(ResponseCacheSetPolicyTest7, FreshPolicyAllowsTinyReuseAfterClear) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  cache.set_policy(std::make_unique<hp::LruEvictionPolicy>());
  cache.store("tinykey", make_entry("value"));
  ASSERT_NE(cache.lookup("tinykey"), nullptr);
}

TEST(ResponseCacheLookupTest5, ConcurrentStoresSerializedOk) {
  const std::string path = make_temp_file_path();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(write_file(path, ini_small_cache()));
  hp::ProxyConfig::instance().load(path);

  hp::ResponseCache cache;
  std::thread t1([&] { cache.store("t1", make_entry("a")); });
  std::thread t2([&] { cache.store("t2", make_entry("b")); });
  t1.join();
  t2.join();
  ASSERT_NE(cache.lookup("t1"), nullptr);
  ASSERT_NE(cache.lookup("t2"), nullptr);
}

} // namespace
