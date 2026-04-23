add_executable(server_integration_tests
    test_client.cpp
    test_server_integration.cpp
)

target_link_libraries(server_integration_tests PRIVATE
    server_lib
    GTest::gtest_main
)

gtest_discover_tests(server_integration_tests)
