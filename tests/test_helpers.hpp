#pragma once

#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

inline std::string make_temp_file_path() {
  char tmpl[] = "/tmp/pks_proxy_testXXXXXX";
  const int fd = mkstemp(tmpl);
  if (fd < 0)
    return {};
  close(fd);
  return std::string(tmpl);
}

inline bool write_file(const std::string &path, const std::string &content) {
  std::ofstream f(path, std::ios::trunc | std::ios::binary);
  if (!f)
    return false;
  f << content;
  return true;
}
