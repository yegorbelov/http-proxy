cmake_minimum_required(VERSION 3.14)
project(pks3_http_proxy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(
  proxy_core STATIC
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

target_include_directories(proxy_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(proxy_core PUBLIC Threads::Threads)

add_executable(target_exec src/main.cpp)
target_link_libraries(target_exec PRIVATE proxy_core)

option(ENABLE_TESTS "Build GoogleTest unit tests and scenario mini-programs" ON)

if(ENABLE_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_subdirectory(tests)
endif()
