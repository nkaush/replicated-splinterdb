find_package(gRPC CONFIG REQUIRED)
find_package(Threads)

# Protobuf/Grpc source files
set(HELLOWORLD_PROTO_FILES helloworld.proto)
set(KVSTORE_PROTO_FILES kvstore.proto)

# Add Library target with protobuf sources
add_library(helloworld ${HELLOWORLD_PROTO_FILES})
add_dependencies(helloworld grpc_cpp_plugin)
target_link_libraries(helloworld PUBLIC protobuf::libprotobuf grpc++)
target_include_directories(helloworld PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Add Library target with protobuf sources
add_library(kvstoreapi ${KVSTORE_PROTO_FILES})
add_dependencies(kvstoreapi grpc_cpp_plugin)
target_link_libraries(kvstoreapi PUBLIC protobuf::libprotobuf grpc++)
target_include_directories(kvstoreapi PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Compile protobuf and grpc files in helloworld target to cpp
cmake_policy(SET CMP0026 OLD)
get_target_property(grpc_cpp_plugin_location grpc_cpp_plugin LOCATION)
cmake_policy(SET CMP0026 NEW)

protobuf_generate(TARGET helloworld LANGUAGE cpp PROTOS ${HELLOWORLD_PROTO_FILES})
protobuf_generate(TARGET helloworld LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")

protobuf_generate(TARGET kvstoreapi LANGUAGE cpp PROTOS ${KVSTORE_PROTO_FILES})
protobuf_generate(TARGET kvstoreapi LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}" PLUGIN_OPTIONS)