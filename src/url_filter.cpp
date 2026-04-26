#include "url_filter.h"

#include <cstddef>

namespace hp {

UrlFilter::UrlFilter(std::vector<std::string> allow, std::vector<std::string> deny)
    : allow_(std::move(allow)), deny_(std::move(deny)) {}

bool UrlFilter::matches_pattern(const std::string& text, const std::string& pattern) {
    if (pattern == "*") return true;
    if (pattern.find('*') == std::string::npos) {
        return text.find(pattern) != std::string::npos;
    }
    bool pfx = pattern[0] == '*';
    bool sfx = !pattern.empty() && pattern.back() == '*';
    std::string mid;
    {
        std::string p = pattern;
        if (pfx) p = p.substr(1);
        if (sfx && !p.empty()) p.pop_back();
        mid = std::move(p);
    }
    if (mid.empty()) return true;
    if (pfx && sfx) return text.find(mid) != std::string::npos;
    if (pfx) {
        return text.size() >= mid.size() && text.compare(text.size() - mid.size(), mid.size(), mid) == 0;
    }
    if (sfx) {
        return text.size() >= mid.size() && text.compare(0, mid.size(), mid) == 0;
    }
    std::size_t star2 = pattern.find('*');
    if (star2 != std::string::npos) {
        const std::string a = pattern.substr(0, star2);
        const std::string b = pattern.size() > star2 + 1 ? pattern.substr(star2 + 1) : std::string();
        if (a.empty() && b.empty()) return true;
        if (a.empty()) return text.find(b) != std::string::npos;
        if (b.empty()) return text.find(a) != std::string::npos;
        std::size_t fa = text.find(a);
        if (fa == std::string::npos) return false;
        return text.find(b, fa + a.size()) != std::string::npos;
    }
    return text.find(mid) != std::string::npos;
}

bool UrlFilter::matches_any(const std::string& url, const std::vector<std::string>& patterns) {
    for (const auto& p : patterns) {
        if (matches_pattern(url, p)) return true;
    }
    return false;
}

bool UrlFilter::deny_logic(const std::string& url) const {
    if (matches_any(url, deny_)) return false;
    if (allow_.empty()) return true;
    if (matches_any(url, allow_)) return true;
    return false;
}

bool UrlFilter::is_allowed(const std::string& url) const { return deny_logic(url); }

}
