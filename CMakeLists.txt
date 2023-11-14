cmake_minimum_required(VERSION 3.7)

project(ReplicatedSplinterDB
        VERSION 1.0
        DESCRIPTION "Replicating SplinterDB with Raft"
        LANGUAGES CXX)

set(CMAKE_BUILD_TYPE Release)

# Let's ensure -std=c++xx instead of -std=g++xx
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)

# Import library shared objects and include directories
link_directories("${CMAKE_CURRENT_BINARY_DIR}")
link_directories("${CMAKE_CURRENT_BINARY_DIR}/third-party/nuraft")

# Include subdirectories with library source code 
set(DISABLE_SSL 1)
add_subdirectory("${ReplicatedSplinterDB_SOURCE_DIR}/third-party/nuraft" EXCLUDE_FROM_ALL)
add_subdirectory("${ReplicatedSplinterDB_SOURCE_DIR}/third-party/rpclib" EXCLUDE_FROM_ALL)

# Include directories
include_directories(
    "${ReplicatedSplinterDB_SOURCE_DIR}/include"
    "${NuRaft_SOURCE_DIR}/include/"
    "${rpc_SOURCE_DIR}/include/"
    "${ReplicatedSplinterDB_SOURCE_DIR}/third-party/splinterdb/include/"
)

add_subdirectory("${ReplicatedSplinterDB_SOURCE_DIR}/apps" EXCLUDE_FROM_ALL)
add_subdirectory("${ReplicatedSplinterDB_SOURCE_DIR}/src/client" EXCLUDE_FROM_ALL)
add_subdirectory("${ReplicatedSplinterDB_SOURCE_DIR}/src/server" EXCLUDE_FROM_ALL)

message(STATUS "Project include path: ${ReplicatedSplinterDB_SOURCE_DIR}/include/")
message(STATUS "NuRaft include path: ${NuRaft_SOURCE_DIR}/include/")
message(STATUS "rpclib include path: ${rpc_SOURCE_DIR}/include/")

set_property(DIRECTORY "${NuRaft_SOURCE_DIR}/tests" PROPERTY EXCLUDE_FROM_ALL True)
set_property(DIRECTORY "${NuRaft_SOURCE_DIR}/examples" PROPERTY EXCLUDE_FROM_ALL True)

add_custom_target(
    clean-splinterdb
    COMMAND ${CMAKE_MAKE_PROGRAM} clean
    WORKING_DIRECTORY ${ReplicatedSplinterDB_SOURCE_DIR}/third-party/splinterdb
)

add_custom_target(
    libsplinterdb.so 
    COMMAND ${CMAKE_MAKE_PROGRAM} LIBDIR=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/libsplinterdb.so
    WORKING_DIRECTORY ${ReplicatedSplinterDB_SOURCE_DIR}/third-party/splinterdb
)

add_custom_target(
    format COMMAND find 
        ${ReplicatedSplinterDB_SOURCE_DIR}/src
        ${ReplicatedSplinterDB_SOURCE_DIR}/apps
        ${ReplicatedSplinterDB_SOURCE_DIR}/include
        -type f "\\(" -iname \*.cpp -o -iname \*.hpp -o -iname \*.h "\\)"
        | xargs clang-format -i
)

add_custom_target(
    tidy COMMAND find 
        ${ReplicatedSplinterDB_SOURCE_DIR}/src
        ${ReplicatedSplinterDB_SOURCE_DIR}/apps
        ${ReplicatedSplinterDB_SOURCE_DIR}/include
        -type f "\\(" -iname \*.cpp -o -iname \*.hpp -o -iname \*.h "\\)"
        | xargs -t clang-tidy -p ${ReplicatedSplinterDB_SOURCE_DIR}
            -extra-arg=-std=c++20
            -extra-arg=-I${ReplicatedSplinterDB_SOURCE_DIR}/include
            -extra-arg=-I${NuRaft_SOURCE_DIR}/include/
            -extra-arg=-I${rpc_SOURCE_DIR}/include/
            -extra-arg=-I${ReplicatedSplinterDB_SOURCE_DIR}/third-party/splinterdb/include/
            -extra-arg=-DSPLINTERDB_PLATFORM_DIR=platform_linux
)