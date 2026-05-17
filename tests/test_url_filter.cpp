#include "url_filter.h"

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

namespace {

using hp::UrlFilter;

using UrlAllowDenyCase =
    std::tuple<std::vector<std::string>, std::vector<std::string>, std::string,
               bool>;

class UrlFilterAllowedTest : public ::testing::TestWithParam<UrlAllowDenyCase> {
};

TEST_P(UrlFilterAllowedTest, IsAllowed_Parameterized) {
  const auto &[allow, deny, url, expected] = GetParam();
  UrlFilter f(allow, deny);
  EXPECT_EQ(f.is_allowed(url), expected);
}

INSTANTIATE_TEST_SUITE_P(
    UrlMatrix, UrlFilterAllowedTest,
    ::testing::Values(
        UrlAllowDenyCase{{}, {}, "http://example.com/foo", true},
        UrlAllowDenyCase{{}, {"bad.com"}, "http://good.com/", true},
        UrlAllowDenyCase{{}, {"bad.com"}, "http://bad.com/page", false},
        UrlAllowDenyCase{{"good.com"}, {}, "http://good.com/", true},
        UrlAllowDenyCase{{"good.com"}, {}, "http://other.com/", false},
        UrlAllowDenyCase{{"nice.com"}, {"evil.com"}, "http://evil.com/", false},
        UrlAllowDenyCase{{"nice.com"}, {"evil.com"}, "http://nice.com/", true},
        UrlAllowDenyCase{{}, {"*"}, "http://anything/", false},
        UrlAllowDenyCase{{"*"}, {}, "http://anything/", true},
        UrlAllowDenyCase{{}, {"pre*post"}, "http://x/prpost/z", true},
        UrlAllowDenyCase{{}, {"pre*post"}, "http://x/preandpost/z", false}));

TEST(UrlFilterManual1, EmptyCtorActsLikeEmptyLists) {
  UrlFilter f;
  EXPECT_TRUE(f.is_allowed("http://example.com"));
}

TEST(UrlFilterManual2, SubstringDenyWithoutStar) {
  UrlFilter f({}, {"spam"});
  EXPECT_FALSE(f.is_allowed("http://spam.example/notreally"));
}

TEST(UrlFilterManual3, AllowListRequiresMatchWhenNonEmpty) {
  UrlFilter f({"only.me"}, {});
  EXPECT_TRUE(f.is_allowed("http://host.only.me/path"));
  EXPECT_FALSE(f.is_allowed("http://only.you/path"));
}

TEST(UrlFilterManual4, DenyOverridesBroadAllowPattern) {
  UrlFilter f({"com"}, {"bad.com"});
  EXPECT_FALSE(f.is_allowed("http://bad.com/"));
}

TEST(UrlFilterManual5, MultipleDenyEntries) {
  UrlFilter f({}, {"a.com", "b.com"});
  EXPECT_FALSE(f.is_allowed("http://a.com/"));
  EXPECT_FALSE(f.is_allowed("http://b.com/"));
  EXPECT_TRUE(f.is_allowed("http://c.com/"));
}

TEST(UrlFilterManual6, StarWildcardMiddleMatchesBothSides) {
  UrlFilter f({}, {"foo*bar"});
  EXPECT_FALSE(f.is_allowed("http://xxfooybarzz/"));
}

TEST(UrlFilterManual7, AllowSeveralPatterns_anyMatches) {
  UrlFilter f({"alpha", "beta"}, {});
  EXPECT_TRUE(f.is_allowed("http://x-alpha-y/"));
  EXPECT_TRUE(f.is_allowed("http://beta/"));
}

} // namespace
