# CMakeLists.txt for the client library
file(GLOB CLIENT_HEADER_LIST CONFIGURE_DEPENDS
    "${ReplicatedSplinterDB_SOURCE_DIR}/include/replicated-splinterdb/client/*.h"
    "${ReplicatedSplinterDB_SOURCE_DIR}/include/replicated-splinterdb/common/*.h")

file(GLOB CLIENT_SOURCE_LIST CONFIGURE_DEPENDS
    "${ReplicatedSplinterDB_SOURCE_DIR}/src/client/*.cpp"
    "${ReplicatedSplinterDB_SOURCE_DIR}/src/common/*.cpp")

# Make the static libraries that are to be linked to the main executables
add_library(replicated-splinterdb-client STATIC ${CLIENT_SOURCE_LIST} ${CLIENT_HEADER_LIST})

# All users of these libraries will need at least C++17
target_compile_features(replicated-splinterdb-client PUBLIC cxx_std_17)

# Add a bunch of warnings for the library build
target_compile_options(
    replicated-splinterdb-client PRIVATE
    -Wall -Wpedantic -Wextra -Wshadow -Wno-unused-parameter
    -Wpointer-arith -Wcast-align -Wwrite-strings -Wredundant-decls 
    -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline
    -Wuninitialized -Wconversion -Wstrict-prototypes -felide-constructors
    -Wno-gcc-compat
)

set_source_files_properties(
    "${ReplicatedSplinterDB_SOURCE_DIR}/src/client/MurmurHash3.cpp"
    PROPERTIES
    COMPILE_FLAGS "-Wno-sign-conversion -Wno-cast-align"
)

# Users of our libraries will need these include directories
target_include_directories(replicated-splinterdb-client PUBLIC "${ReplicatedSplinterDB_SOURCE_DIR}/include/")

# Link the libraries to some other dependencies
target_link_libraries(replicated-splinterdb-client rpc pthread)
