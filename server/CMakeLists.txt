add_library(server_lib STATIC
    src/server_config.cpp
    src/async_logger.cpp
    src/log_sink.cpp
    src/client_session.cpp
    src/tcp_server.cpp
    src/message_handler.cpp
    src/document_manager.cpp
    src/document_session.cpp
    src/cursor_aggregator.cpp
    src/autosave.cpp
)

target_include_directories(server_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(server_lib PUBLIC
    collab::core
    collab::protocol
    Boost::boost
    nlohmann_json::nlohmann_json
)

target_compile_features(server_lib PUBLIC cxx_std_20)

add_executable(collab_server src/main.cpp)
target_link_libraries(collab_server PRIVATE server_lib)
