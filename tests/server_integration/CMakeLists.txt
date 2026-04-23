add_executable(server_integration_tests
    test_client.cpp
    test_server_integration.cpp
)

target_link_libraries(server_integration_tests PRIVATE
    server_lib
    GTest::gtest_main
)

target_include_directories(server_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

gtest_discover_tests(server_integration_tests)
