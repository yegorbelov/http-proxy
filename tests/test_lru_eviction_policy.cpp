#include "ieviction_policy.h"

#include <gtest/gtest.h>

namespace {

using hp::LruEvictionPolicy;

TEST(LruTouchTest1, SingleKeyBecomesFrontWhenPickLru) {
  LruEvictionPolicy p;
  p.touch("a");
  EXPECT_EQ(p.pick_lru(), "a");
}

TEST(LruTouchTest2, SecondOlderKeyIsLruWhenBothTouchedInOrder) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  EXPECT_EQ(p.pick_lru(), "a");
}

TEST(LruTouchTest3, ReTouchMovesKeyToMostRecent) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  p.touch("a");
  EXPECT_EQ(p.pick_lru(), "b");
}

TEST(LruTouchTest4, TripleChainOrder) {
  LruEvictionPolicy p;
  p.touch("x");
  p.touch("y");
  p.touch("z");
  EXPECT_EQ(p.pick_lru(), "x");
}

TEST(LruTouchTest5, DuplicateTouchKeepsSingleOccurrence) {
  LruEvictionPolicy p;
  p.touch("k");
  p.touch("k");
  EXPECT_EQ(p.pick_lru(), "k");
}

TEST(LruTouchTest6, LongSequenceOldestStable) {
  LruEvictionPolicy p;
  for (char c = 'a'; c <= 'f'; ++c) {
    p.touch(std::string(1, c));
  }
  EXPECT_EQ(p.pick_lru(), "a");
}

TEST(LruTouchTest7, TouchAfterClearRebuildsOrder) {
  LruEvictionPolicy p;
  p.touch("a");
  p.clear();
  p.touch("n");
  EXPECT_EQ(p.pick_lru(), "n");
}

TEST(LruOnRemoveTest1, RemoveMissingNoCrashPickEmpty) {
  LruEvictionPolicy p;
  p.on_remove("ghost");
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruOnRemoveTest2, RemoveExistingUpdatesHead) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  p.on_remove("a");
  EXPECT_EQ(p.pick_lru(), "b");
}

TEST(LruOnRemoveTest3, RemoveMostRecentLeavesOlderIntact) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  p.on_remove("b");
  EXPECT_EQ(p.pick_lru(), "a");
}

TEST(LruOnRemoveTest4, RemoveOnlyElementEmptyPick) {
  LruEvictionPolicy p;
  p.touch("solo");
  p.on_remove("solo");
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruOnRemoveTest5, RemoveMiddleKey) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  p.touch("c");
  p.on_remove("b");
  EXPECT_EQ(p.pick_lru(), "a");
  p.touch("d");
  EXPECT_EQ(p.pick_lru(), "a");
}

TEST(LruOnRemoveTest6, DoubleRemoveIdempotentForPick) {
  LruEvictionPolicy p;
  p.touch("a");
  p.on_remove("a");
  p.on_remove("a");
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruOnRemoveTest7, TouchAfterRemoveRestoresKey) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  p.on_remove("a");
  p.touch("a");
  EXPECT_EQ(p.pick_lru(), "b");
}

TEST(LruPickLruTest1, EmptyInitially) {
  LruEvictionPolicy p;
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruPickLruTest2, SingleTouchReturnsThatKey) {
  LruEvictionPolicy p;
  p.touch("k");
  EXPECT_EQ(p.pick_lru(), "k");
}

TEST(LruPickLruTest3, PickDoesNotMutateOrder) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  EXPECT_EQ(p.pick_lru(), "a");
  EXPECT_EQ(p.pick_lru(), "a");
}

TEST(LruPickLruTest4, AfterClearPickEmpty) {
  LruEvictionPolicy p;
  p.touch("z");
  p.clear();
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruPickLruTest5, LastInsertedNotLruUnlessOldest) {
  LruEvictionPolicy p;
  p.touch("old");
  p.touch("mid");
  p.touch("new");
  EXPECT_EQ(p.pick_lru(), "old");
}

TEST(LruPickLruTest6, UnicodeLikeKeysStillOrdered) {
  LruEvictionPolicy p;
  p.touch("\x01");
  p.touch("\x02");
  EXPECT_EQ(p.pick_lru(), std::string("\x01"));
}

TEST(LruPickLruTest7, LongKeyStable) {
  LruEvictionPolicy p;
  const std::string long_k(400, 'x');
  p.touch(long_k);
  EXPECT_EQ(p.pick_lru(), long_k);
}

TEST(LruClearTest1, ClearEmptySafe) {
  LruEvictionPolicy p;
  p.clear();
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruClearTest2, ClearAfterTouches) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  p.clear();
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruClearTest3, TouchAfterClearFreshState) {
  LruEvictionPolicy p;
  p.touch("a");
  p.clear();
  p.touch("b");
  p.touch("c");
  EXPECT_EQ(p.pick_lru(), "b");
}

TEST(LruClearTest4, RemoveAfterClearNoOp) {
  LruEvictionPolicy p;
  p.touch("a");
  p.clear();
  p.on_remove("a");
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruClearTest5, MultipleClearsIdempotent) {
  LruEvictionPolicy p;
  p.touch("x");
  p.clear();
  p.clear();
  EXPECT_TRUE(p.pick_lru().empty());
}

TEST(LruClearTest6, ClearThenReuseSameKeyName) {
  LruEvictionPolicy p;
  p.touch("reuse");
  p.clear();
  p.touch("reuse");
  EXPECT_EQ(p.pick_lru(), "reuse");
}

TEST(LruClearTest7, ClearBetweenBursts) {
  LruEvictionPolicy p;
  p.touch("a");
  p.touch("b");
  p.clear();
  p.touch("c");
  EXPECT_EQ(p.pick_lru(), "c");
}

} // namespace
