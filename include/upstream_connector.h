#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace hp {

struct ParsedHttpUrl {
  std::string scheme{"http"};
  std::string host;
  int port{80};
  std::string path_and_query;
};

struct FetchResult {
  bool ok{false};
  int status{0};
  std::string body;
  std::string raw_header_block;
  std::map<std::string, std::string> response_headers_lower;
  std::string err;
};

struct FetchRequest {
  std::string method;
  ParsedHttpUrl target;
  std::vector<std::pair<std::string, std::string>> client_headers;
  std::string request_body;
};

class UpstreamConnector {
public:
  static bool parse_http_proxy_target(const std::string &first_line,
                                      const std::string &request_headers,
                                      FetchRequest &out, std::string &error);
  static bool parse_url_absolute(const std::string &abs_uri, ParsedHttpUrl &out,
                                 std::string &err);
  static FetchResult fetch(const FetchRequest &req, long long max_body);
};

} // namespace hp
