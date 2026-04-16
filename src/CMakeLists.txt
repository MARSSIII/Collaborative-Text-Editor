add_library(collab STATIC
    operation.cpp
    ot.cpp
    document.cpp
    thread_pool.cpp
    access_control.cpp
    auth_manager.cpp
    hash_utils.cpp
    token_bucket.cpp
)

target_include_directories(collab PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)

target_compile_features(collab PUBLIC cxx_std_20)

add_library(collab::collab ALIAS collab)
