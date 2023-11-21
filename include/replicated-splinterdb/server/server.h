#ifndef REPLICATED_SPLINTERDB_SERVER_SERVER_H
#define REPLICATED_SPLINTERDB_SERVER_SERVER_H

#include <grpcpp/grpcpp.h>

#include "kvstore.grpc.pb.h"
#include "replicated-splinterdb/server/replica.h"
#include "replicated-splinterdb/server/replica_config.h"
#include "rpc/server.h"
#include "rpc/this_handler.h"

namespace replicated_splinterdb {

class server {
  public:
    server() = delete;

    ~server();

    server(const server&) = delete;

    server& operator=(const server&) = delete;

    server(uint16_t client_port, uint16_t join_port, const replica_config& cfg);

    void run(uint64_t nthreads);

  private:
    replica replica_instance_;

    uint16_t client_port_;

    std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> cqs_;
    kvstore::ReplicatedKVStore::AsyncService service_;
    std::unique_ptr<grpc::Server> server_;

    rpc::server join_srv_;

    void initialize();

    void HandleRpcs(grpc::ServerCompletionQueue* cq);
};

}  // namespace replicated_splinterdb

#endif  // REPLICATED_SPLINTERDB_SERVER_SERVER_H