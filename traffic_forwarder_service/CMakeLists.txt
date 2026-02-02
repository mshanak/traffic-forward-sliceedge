cmake_minimum_required(VERSION 3.10)
project(port_service_forwarder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(port_service_forwarder
    src/main.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(port_service_forwarder PRIVATE Threads::Threads)

# Force static linking
set_target_properties(port_service_forwarder PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
)

# Add flags for static libc
target_link_options(port_service_forwarder PRIVATE -static -static-libgcc -static-libstdc++)
