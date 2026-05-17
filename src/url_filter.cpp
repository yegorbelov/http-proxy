#include "url_filter.h"

namespace hp {

UrlFilter::UrlFilter(std::vector<std::string> allow,
                     std::vector<std::string> deny)
    : allow_(std::move(allow)), deny_(std::move(deny)) {}

bool UrlFilter::matches_pattern(const std::string &text,
                                const std::string &pattern) {
  if (pattern == "*")
    return true;
  if (pattern.find('*') == std::string::npos) {
    return text.find(pattern) != std::string::npos;
  }
  size_t star = pattern.find('*');
  std::string before = pattern.substr(0, star);
  std::string after = star + 1 < pattern.size() ? pattern.substr(star + 1) : "";
  if (!before.empty() && text.find(before) == std::string::npos)
    return false;
  if (!after.empty() && text.find(after) == std::string::npos)
    return false;
  return true;
}

bool UrlFilter::matches_any(const std::string &url,
                            const std::vector<std::string> &patterns) {
  for (const auto &p : patterns) {
    if (matches_pattern(url, p))
      return true;
  }
  return false;
}

bool UrlFilter::deny_logic(const std::string &url) const {
  if (matches_any(url, deny_))
    return false;
  if (allow_.empty())
    return true;
  if (matches_any(url, allow_))
    return true;
  return false;
}

bool UrlFilter::is_allowed(const std::string &url) const {
  return deny_logic(url);
}

} // namespace hp
