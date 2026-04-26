#include "proxy_config.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace hp {

void ProxyConfig::set_defaults() {
    listen_host_ = "0.0.0.0";
    listen_port_ = 8080;
    cache_max_size_mb_ = 256;
    cache_max_entry_size_mb_ = 10;
    default_ttl_sec_ = 300;
    eviction_mode_ = 0;
    log_path_ = "proxy.log";
    admin_reload_enabled_ = true;
    allow_url_patterns_.clear();
    deny_url_patterns_.clear();
}

ProxyConfig& ProxyConfig::instance() {
    static ProxyConfig c;
    return c;
}

int ProxyConfig::parse_int(const std::string& v, int def, int min_v, int max_v) {
    if (v.empty()) return def;
    char* endp = nullptr;
    long n = std::strtol(v.c_str(), &endp, 10);
    if (endp == v.c_str() || n < min_v) return def;
    if (n > max_v) return static_cast<int>(std::min<long>(n, max_v));
    return static_cast<int>(n);
}

void ProxyConfig::parse_list_value(const std::string& s, char delim, std::vector<std::string>& out) {
    out.clear();
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            if (!item.empty() && item.back() == '\r') item.pop_back();
            out.push_back(item);
        }
    }
    if (out.empty() && !s.empty() && s.find(delim) == std::string::npos) {
        if (!s.empty() && s.back() == '\r') {
            out.push_back(s.substr(0, s.size() - 1));
        } else
            out.push_back(s);
    }
}

void ProxyConfig::apply_line(const std::string& line, std::size_t nline) {
    if (line.empty() || (line[0] == '#')) return;
    auto p = line.find('=');
    if (p == std::string::npos) return;
    std::string key = line.substr(0, p);
    std::string value = (p + 1 < line.size()) ? line.substr(p + 1) : std::string{};
    while (!key.empty() && (key.back() == ' ' || key.back() == '\r')) key.pop_back();
    while (!value.empty() && (value.front() == ' ')) value.erase(value.begin());
    if (!value.empty() && value.back() == '\r') value.pop_back();

    if (key == "listen_host")
        listen_host_ = value;
    else if (key == "listen_port")
        listen_port_ = parse_int(value, 8080, 1, 65535);
    else if (key == "cache_max_size_mb")
        cache_max_size_mb_ = parse_int(value, 256, 1, 1 << 20);
    else if (key == "cache_max_entry_size_mb")
        cache_max_entry_size_mb_ = parse_int(value, 10, 1, 1 << 10);
    else if (key == "default_ttl_sec")
        default_ttl_sec_ = parse_int(value, 300, 1, 1 << 20);
    else if (key == "eviction_policy")
        (void)0;
    else if (key == "log_path")
        log_path_ = value;
    else if (key == "admin_reload_enabled")
        admin_reload_enabled_ = (value == "1" || value == "true" || value == "True");
    else if (key == "allow_url_patterns")
        parse_list_value(value, ';', allow_url_patterns_);
    else if (key == "deny_url_patterns")
        parse_list_value(value, ';', deny_url_patterns_);
    (void)nline;
}

void ProxyConfig::load(const std::string& path) {
    set_defaults();
    std::ifstream f(path);
    if (!f) {
        std::cerr << "pks2-http-proxy: no config " << path << " — defaults\n";
        return;
    }
    std::string line;
    std::size_t n = 0;
    while (std::getline(f, line)) {
        ++n;
        apply_line(line, n);
    }
}

std::int64_t ProxyConfig::cacheMaxBytes() const {
    return static_cast<std::int64_t>(cache_max_size_mb_) * 1024 * 1024;
}

std::int64_t ProxyConfig::cacheMaxEntryBytes() const {
    return static_cast<std::int64_t>(cache_max_entry_size_mb_) * 1024 * 1024;
}

}  // namespace hp
