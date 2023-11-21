#include "replicated-splinterdb/server/server.h"

#include <iostream>

#include "call_data.h"
#include "libnuraft/buffer_serializer.hxx"
#include "replicated-splinterdb/common/rpc.h"
#include "replicated-splinterdb/common/types.h"

#define TPCQ (4)

namespace replicated_splinterdb {

using nuraft::buffer;
using nuraft::cmd_result_code;
using nuraft::ptr;
using std::string;
using std::vector;

server::server(uint16_t client_port, uint16_t join_port,
               const replica_config& cfg)
    : replica_instance_{cfg}, client_port_(client_port), join_srv_{join_port} {
    initialize();
}

server::~server() {
    server_->Shutdown();
    // Always shutdown the completion queue after the server.
    for (auto& cq : cqs_) {
        cq->Shutdown();
    }

    join_srv_.stop();
    replica_instance_.shutdown(5);
}

void server::run(uint64_t nthreads) {
    std::string server_address = "0.0.0.0:" + std::to_string(client_port_);

    grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service_" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *asynchronous* service.
    builder.RegisterService(&service_);
    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.
    for (uint64_t i = 0; i < (nthreads / TPCQ); ++i) {
        cqs_.push_back(builder.AddCompletionQueue());
    }
    // cqs_.push_back(builder.AddCompletionQueue());

    // Finally assemble the server.
    server_ = builder.BuildAndStart();
    std::cout << "Listening for client RPCs on " << server_address << std::endl;

    std::vector<std::thread> threads;
    for (uint64_t i = 0; i < nthreads; ++i) {
        std::thread t([i, this]() {
            replica_instance_.register_thread();
            HandleRpcs(cqs_[i / TPCQ].get());
            // HandleRpcs(cqs_[0].get());
        });
        threads.push_back(std::move(t));
    }

    std::cout << "Listening for cluster join RPCs on port " << join_srv_.port()
              << std::endl;

    join_srv_.async_run(1);

    for (auto& th : threads) {
        th.join();
    }
}

void server::HandleRpcs(grpc::ServerCompletionQueue* cq) {
    // Spawn a new CallData instance to serve new clients.
    new CallDataGet(&service_, cq, replica_instance_);
    new CallDataPut(&service_, cq, replica_instance_);
    new CallDataUpdate(&service_, cq, replica_instance_);
    new CallDataDelete(&service_, cq, replica_instance_);
    new CallDataGetLeaderID(&service_, cq, replica_instance_);
    new CallDataGetClusterEndpoints(&service_, cq, replica_instance_);
    void* tag;  // uniquely identifies a request.
    bool ok;
    while (true) {
        // Block waiting to read the next event from the completion queue.
        // The event is uniquely identified by its tag, which in this case
        // is the memory address of a CallData instance. The return value of
        // Next should always be checked. This return value tells us whether
        // there is any kind of event or cq_ is shutting down.
        bool ret = cq->Next(&tag, &ok);
        if (ok == false || ret == false) {
            return;
        }
        static_cast<CallDataBase*>(tag)->Proceed(replica_instance_);
    }
}

void server::initialize() {
    // (int32_t, std::string, std::string) -> (int32_t, std::string)
    join_srv_.bind(RPC_JOIN_REPLICA_GROUP,
                   [this](int32_t server_id, std::string raft_endpoint,
                          std::string client_endpoint) {
                       auto [rc, msg] = replica_instance_.add_server(
                           server_id, raft_endpoint, client_endpoint);
                       return std::make_tuple(static_cast<int32_t>(rc), msg);
                   });
}

}  // namespace replicated_splinterdb