add_library(collab_core STATIC
    src/operation.cpp
    src/ot.cpp
    src/document.cpp
    src/thread_pool.cpp
    src/access_control.cpp
    src/auth_manager.cpp
    src/hash_utils.cpp
    src/token_bucket.cpp
)

target_include_directories(collab_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(collab_core PUBLIC cxx_std_20)

add_library(collab::core ALIAS collab_core)
