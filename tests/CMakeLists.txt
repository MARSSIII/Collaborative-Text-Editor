add_executable(business_logic_tests
    test_ot_transform.cpp
    test_ot_apply.cpp
    test_ot_convergence.cpp
    test_ot_server_pipeline.cpp
    test_document_lifecycle.cpp
    test_document_threadsafety.cpp
    test_access_control.cpp
    test_auth.cpp
    test_mpsc_queue.cpp
    test_token_bucket.cpp
    test_thread_pool.cpp
)

target_link_libraries(business_logic_tests PRIVATE
    collab::collab
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(business_logic_tests)

if(TARGET server_lib)
    add_executable(server_integration_tests
        server_integration/test_client.cpp
        server_integration/test_server_integration.cpp
    )
    target_link_libraries(server_integration_tests PRIVATE
        server_lib
        GTest::gtest_main
    )
    gtest_discover_tests(server_integration_tests)
endif()
