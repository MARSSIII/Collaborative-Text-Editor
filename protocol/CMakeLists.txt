add_library(collab_protocol STATIC
    src/protocol.cpp
)

target_include_directories(collab_protocol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(collab_protocol PUBLIC
    nlohmann_json::nlohmann_json
)

target_compile_features(collab_protocol PUBLIC cxx_std_20)

add_library(collab::protocol ALIAS collab_protocol)
