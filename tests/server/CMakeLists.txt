add_executable(server_tests
    test_auth.cpp
    test_access_control.cpp
    test_thread_pool.cpp
    test_token_bucket.cpp
)

target_link_libraries(server_tests PRIVATE
    collab::core
    GTest::gtest_main
)

target_include_directories(server_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

gtest_discover_tests(server_tests)
