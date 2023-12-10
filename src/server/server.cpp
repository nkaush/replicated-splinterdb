#include "replicated-splinterdb/server/server.h"

#include <iostream>

#include "libnuraft/buffer_serializer.hxx"
#include "replicated-splinterdb/common/rpc.h"
#include "replicated-splinterdb/common/types.h"

namespace replicated_splinterdb {

using nuraft::buffer;
using nuraft::cmd_result_code;
using nuraft::ptr;
using std::string;
using std::vector;

server::server(uint16_t client_port, uint16_t join_port,
               const replica_config& cfg)
    : replica_instance_{cfg}, client_srv_{cfg.addr_, client_port}, join_srv_{cfg.addr_, join_port} {
    initialize();

    client_srv_.set_worker_init_func(
        [this] { replica_instance_.register_thread(); });
}

server::~server() {
    client_srv_.stop();
    join_srv_.stop();
    replica_instance_.shutdown(5);
}

void server::run(uint64_t nthreads) {
    client_srv_.async_run(static_cast<size_t>(nthreads));
    std::cout << "Listening for client RPCs on port " << client_srv_.port()
              << std::endl;

    std::cout << "Listening for cluster join RPCs on port " << join_srv_.port()
              << std::endl;

    join_srv_.run();
}

static rpc_mutation_result extract_result(ptr<replica::raft_result> result) {
    int32_t spl_rc = 0;
    int32_t raft_rc = 999;

    if (!result->get_accepted()) {
        std::cout << "WARNING: log append failed." << std::endl;
        raft_rc = result->get_result_code();
    } else if (!result->has_result()) {
        std::cout << "WARNING: SM did not yield result yet" << std::endl;
        raft_rc = result->get_result_code();
    } else {
        raft_rc = result->get_result_code();
        ptr<buffer> buf = result->get();

        if (buf != nullptr) {
            spl_rc = buf->get_int();
        } else {
            std::cout << "WARNING: GOT nullptr RESULT (raft_rc=" << raft_rc
                      << ", " << result->get_result_str() << ")" << std::endl;
        }
    }

    return {spl_rc, raft_rc, result->get_result_str()};
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

    // string -> bool
    client_srv_.bind(RPC_SPLINTERDB_DUMPCACHE, [this](std::string directory) {
        replica_instance_.dump_cache(directory);
        return true;
    });

    // void -> bool
    client_srv_.bind(RPC_SPLINTERDB_CLEARCACHE, [this]() {
        std::cout << "Clearing splinterdb cache ... " << std::flush;
        replica_instance_.clear_cache();
        std::cout << "done." << std::endl;
        return true;
    });

    // void -> std::string
    client_srv_.bind(RPC_PING, []() { return "pong"; });

    // void -> int32_t
    client_srv_.bind(RPC_GET_SRV_ID,
                     [this]() { return replica_instance_.get_id(); });

    // void -> int32_t
    client_srv_.bind(RPC_GET_LEADER_ID,
                     [this]() { return replica_instance_.get_leader(); });

    // void -> rpc_cluster_endpoints
    client_srv_.bind(RPC_GET_ALL_SERVERS, [this]() {
        std::vector<ptr<nuraft::srv_config>> configs;
        replica_instance_.get_all_servers(configs);

        std::vector<rpc_server_info> result;
        for (auto& srv : configs) {
            result.emplace_back(srv->get_id(), srv->get_aux());
        }

        return rpc_cluster_endpoints{std::move(result)};
    });

    // string -> rpc_read_result
    client_srv_.bind(RPC_SPLINTERDB_GET, [this](string key) {
        slice key_slice = slice_create(key.size(), key.data());
        auto [data, rc] = replica_instance_.read(std::move(key_slice));

        if (rc == 0) {
            return rpc_read_result{std::move(data), 0};
        } else {
            return rpc_read_result{rc};
        }
    });

    // (string, string) -> rpc_mutation_result
    client_srv_.bind(RPC_SPLINTERDB_PUT, [this](string key, string value) {
        splinterdb_operation op{
            splinterdb_operation::make_put(std::move(key), std::move(value))};
        ptr<replica::raft_result> result = replica_instance_.append_log(op);

        return extract_result(result);
    });

    // string -> rpc_mutation_result
    client_srv_.bind(RPC_SPLINTERDB_DELETE, [this](string key) {
        splinterdb_operation op{
            splinterdb_operation::make_delete(std::move(key))};
        ptr<replica::raft_result> result = replica_instance_.append_log(op);

        return extract_result(result);
    });

    // (string, string) -> rpc_mutation_result
    client_srv_.bind(RPC_SPLINTERDB_UPDATE, [this](string key, string value) {
        splinterdb_operation op{
            splinterdb_operation::make_put(std::move(key), std::move(value))};
        ptr<replica::raft_result> result = replica_instance_.append_log(op);

        return extract_result(result);
    });
}

}  // namespace replicated_splinterdb