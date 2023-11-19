file(GLOB SERVER_HEADER_LIST CONFIGURE_DEPENDS
    "${ReplicatedSplinterDB_SOURCE_DIR}/include/replicated-splinterdb/server/*.h"
    "${ReplicatedSplinterDB_SOURCE_DIR}/include/replicated-splinterdb/common/*.h")

file(GLOB SERVER_SOURCE_LIST CONFIGURE_DEPENDS
    "${ReplicatedSplinterDB_SOURCE_DIR}/src/server/*.cpp"
    "${ReplicatedSplinterDB_SOURCE_DIR}/src/common/*.cpp")

# Make the static libraries that are to be linked to the main executables
add_library(replicated-splinterdb-server STATIC EXCLUDE_FROM_ALL ${SERVER_SOURCE_LIST} ${SERVER_HEADER_LIST})
add_dependencies(replicated-splinterdb-server libsplinterdb.so)
add_dependencies(replicated-splinterdb-server NuRaft::static_lib)

# All users of these libraries will need at least C++17
target_compile_features(replicated-splinterdb-server PUBLIC cxx_std_17)

# All users of the server library will need to define SPLINTERDB_PLATFORM_DIR
target_compile_definitions(replicated-splinterdb-server PUBLIC -DSPLINTERDB_PLATFORM_DIR=platform_linux)

# Add a bunch of warnings for the library build
target_compile_options(
    replicated-splinterdb-server PRIVATE
    -Wall -Wpedantic -Wextra -Wshadow -Wno-unused-parameter
    -Wpointer-arith -Wcast-align -Wwrite-strings -Wredundant-decls 
    -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline
    -Wuninitialized -Wconversion -Wstrict-prototypes -felide-constructors
    -Wno-gcc-compat
)

set_source_files_properties(
    "${ReplicatedSplinterDB_SOURCE_DIR}/src/server/logger.cpp"
    PROPERTIES COMPILE_FLAGS "-Wno-sign-conversion -Wshorten-64-to-32"
)

# Link the libraries to some other dependencies
target_link_libraries(replicated-splinterdb-server nuraft.a rpc splinterdb pthread grpc++ kvstoreapi)
