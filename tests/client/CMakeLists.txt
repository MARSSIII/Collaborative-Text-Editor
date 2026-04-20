add_executable(client_codec_tests
    test_protocol_codec.cpp
    test_utf8_codec.cpp
    test_local_document.cpp
    test_cursor_translator.cpp
)
target_link_libraries(client_codec_tests PRIVATE
    collab_client_lib
    collab::collab
    collab::protocol
    Qt5::Core
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

add_executable(client_editor_tests
    test_editor_widget.cpp
)
set_target_properties(client_editor_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_editor_tests PRIVATE
    collab_client_lib
    collab::collab
    Qt5::Widgets
    Qt5::Test
)
add_test(NAME client_editor_tests COMMAND client_editor_tests -platform offscreen)

add_executable(client_ot_controller_tests
    test_ot_controller.cpp
)
set_target_properties(client_ot_controller_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_ot_controller_tests PRIVATE
    collab_client_lib
    collab::collab
    collab::protocol
    Qt5::Core
    Qt5::Network
    Qt5::Test
)
add_test(NAME client_ot_controller_tests COMMAND client_ot_controller_tests)
