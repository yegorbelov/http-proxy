#include "upstream_connector.h"

#include <iostream>
#include <string>

int main() {
  std::string request_line;
  if (!std::getline(std::cin, request_line)) {
    std::cerr << "missing request line on stdin\n";
    return 1;
  }
  std::string headers_block;
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty())
      break;
    headers_block += line + "\r\n";
  }
  hp::FetchRequest fr;
  std::string err;
  if (!hp::UpstreamConnector::parse_http_proxy_target(request_line,
                                                      headers_block, fr, err)) {
    std::cerr << "parse_error: " << err << '\n';
    return 2;
  }
  std::cout << "method=" << fr.method << '\n';
  std::cout << "host=" << fr.target.host << '\n';
  std::cout << "port=" << fr.target.port << '\n';
  std::cout << "path=" << fr.target.path_and_query << '\n';
  return 0;
}
