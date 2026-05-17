#include "url_filter.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split_semicolon(const std::string &s) {
  std::vector<std::string> out;
  if (s.empty() || s == "-")
    return out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ';')) {
    if (!item.empty())
      out.push_back(item);
  }
  return out;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "usage: scenario_url_policy <allow_patterns;- if empty> "
                 "<deny_patterns;- if empty> <full_http_url>\n";
    return 1;
  }
  const std::string allow_raw = argv[1];
  const std::string deny_raw = argv[2];
  const std::string url = argv[3];
  hp::UrlFilter filter(split_semicolon(allow_raw == "-" ? "" : allow_raw),
                       split_semicolon(deny_raw == "-" ? "" : deny_raw));
  std::cout << (filter.is_allowed(url) ? "allowed" : "blocked") << '\n';
  return 0;
}
