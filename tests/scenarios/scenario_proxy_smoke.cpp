#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: scenario_proxy_smoke <proxy http://host:port> "
                 "<target http URL>\n";
    return 1;
  }
  const std::string cmd =
      std::string("curl -s -o /dev/null -w %{http_code} --max-time 15 -x ") +
      argv[1] + ' ' + argv[2];
  std::unique_ptr<FILE, int (*)(FILE *)> pipe(popen(cmd.c_str(), "r"), &pclose);
  if (!pipe) {
    std::cerr << "popen failed (install curl)\n";
    return 3;
  }
  std::array<char, 64> buf{};
  if (!std::fgets(buf.data(), static_cast<int>(buf.size()), pipe.get())) {
    std::cerr << "no curl output\n";
    return 4;
  }
  std::cout << "http_code=" << buf.data();
  return 0;
}
