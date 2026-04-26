cmake_minimum_required(VERSION 3.10)
project(pks2_http_proxy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(
  target_exec
  src/main.cpp
  src/proxy_config.cpp
  src/url_filter.cpp
  src/ieviction_policy.cpp
  src/response_cache.cpp
  src/event_log.cpp
  src/upstream_connector.cpp
  src/http_client_session.cpp
  src/client_session_factory.cpp
  src/http_proxy_server.cpp
)

target_include_directories(target_exec PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(target_exec PRIVATE Threads::Threads)
