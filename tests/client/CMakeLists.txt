add_executable(client_codec_tests
    test_protocol_codec.cpp
)
target_link_libraries(client_codec_tests PRIVATE
    collab_client_lib
    collab::protocol
    GTest::gtest_main
)
gtest_discover_tests(client_codec_tests)

add_executable(client_network_tests
    test_network_manager.cpp
)
set_target_properties(client_network_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_network_tests PRIVATE
    collab_client_lib
    collab::protocol
    Qt5::Core
    Qt5::Network
    Qt5::Test
)
add_test(NAME client_network_tests COMMAND client_network_tests)
