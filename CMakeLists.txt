cmake_minimum_required(VERSION 3.0.0)
project(CenterManager VERSION 0.1.0)

include(CTest)
enable_testing()

# Add the third-party headers directory to the search path
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/external")

# Compile jsoncpp from source
add_library(jsoncpp STATIC ${PROJECT_SOURCE_DIR}/external/jsoncpp.cpp)

add_executable(CenterManager main.cpp websocketserver.cpp)
target_link_libraries (CenterManager jsoncpp ssl pthread crypto)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
