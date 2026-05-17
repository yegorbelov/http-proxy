add_executable(
  proxy_unit_tests
  test_url_filter.cpp
  test_lru_eviction_policy.cpp
  test_proxy_config.cpp
  test_response_cache.cpp
  test_event_log.cpp
  test_upstream_connector_parse.cpp
  test_http_client_session_socketpair.cpp
)

target_include_directories(proxy_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(proxy_unit_tests PRIVATE proxy_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(
  proxy_unit_tests
  PROPERTIES
    RUN_SERIAL
    TRUE
)

macro(add_scenario exe stem)
  add_executable(${exe} scenarios/${stem}.cpp)
  target_include_directories(${exe} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${exe} PRIVATE proxy_core)
endmacro()

add_scenario(scenario_config_dump scenario_config_dump)
add_scenario(scenario_url_policy scenario_url_policy)
add_scenario(scenario_cache_demo scenario_cache_demo)
add_scenario(scenario_parse_request scenario_parse_request)

add_executable(scenario_proxy_smoke scenarios/scenario_proxy_smoke.cpp)
