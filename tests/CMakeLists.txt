include(GoogleTest)

add_subdirectory(core)
add_subdirectory(server)

if(TARGET server_lib)
    add_subdirectory(server_integration)
endif()

if(TARGET collab_client_lib)
    add_subdirectory(client)
endif()
