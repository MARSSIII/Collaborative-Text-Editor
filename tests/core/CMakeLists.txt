add_executable(core_tests
    test_ot_transform.cpp
    test_ot_apply.cpp
    test_ot_convergence.cpp
    test_ot_server_pipeline.cpp
    test_document_lifecycle.cpp
    test_document_threadsafety.cpp
    test_mpsc_queue.cpp
)

target_link_libraries(core_tests PRIVATE
    collab::core
    GTest::gtest_main
)

target_include_directories(core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

gtest_discover_tests(core_tests)
