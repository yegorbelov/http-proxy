#pragma once

#include <string>
#include <vector>

namespace hp {

class UrlFilter {
   public:
    UrlFilter() = default;
    UrlFilter(std::vector<std::string> allow, std::vector<std::string> deny);
    bool is_allowed(const std::string& url) const;

   private:
    static bool matches_pattern(const std::string& text, const std::string& pattern);
    static bool matches_any(const std::string& url, const std::vector<std::string>& patterns);
    bool deny_logic(const std::string& url) const;

    std::vector<std::string> allow_;
    std::vector<std::string> deny_;
};

}
