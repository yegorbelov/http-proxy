#include "http_proxy_server.h"
#include "proxy_config.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string p = (argc > 1) ? argv[1] : "config/default_config.ini";
    hp::ProxyConfig::instance().load(p);
    hp::HttpProxyServer srv;
    const int rc = srv.run();
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
