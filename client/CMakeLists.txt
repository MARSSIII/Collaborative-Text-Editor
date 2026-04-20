set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Network Test)

add_library(collab_client_lib STATIC
    src/network_manager.cpp
    src/protocol_codec.cpp
    src/connection_dialog.cpp
    src/document_list_window.cpp
    src/utf8_codec.cpp
    src/cursor_translator.cpp
    src/local_document.cpp
    src/ot_controller.cpp
    src/editor_widget.cpp
    src/editor_window.cpp
    src/status_bar_widget.cpp
    src/remote_cursors_model.cpp
    src/user_panel_widget.cpp
    include/client/network_manager.h
    include/client/connection_dialog.h
    include/client/document_list_window.h
    include/client/ot_controller.h
    include/client/editor_widget.h
    include/client/editor_window.h
    include/client/status_bar_widget.h
    include/client/remote_cursors_model.h
    include/client/user_panel_widget.h
)

set_target_properties(collab_client_lib PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_include_directories(collab_client_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(collab_client_lib PUBLIC
    collab::collab
    collab::protocol
    Qt5::Core
    Qt5::Network
    Qt5::Widgets
)

target_compile_features(collab_client_lib PUBLIC cxx_std_20)

add_executable(collab_client
    src/main.cpp
)

target_link_libraries(collab_client PRIVATE
    collab_client_lib
    Qt5::Widgets
)
