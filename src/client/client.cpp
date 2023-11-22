#include "replicated-splinterdb/client/client.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "replicated-splinterdb/common/rpc.h"

// TODOS: Implement latency-based read policy

#define GET_LEADER_NO_LIVE_LEADER (-1)
#define CMD_RESULT_NOT_LEADER (-3)
#define CMD_RESULT_REQUEST_CANCELLED (-1)
#define CMD_RESULT_WEIRD_CASE (999)

namespace replicated_splinterdb {

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using kvstore::ClientFacingEndpoint;
using kvstore::ClusterEndpoints;
using kvstore::Empty;
using kvstore::Key;
using kvstore::KVPair;
using kvstore::MutationResponse;
using kvstore::ReadResponse;
using kvstore::ReplicatedKVStore;
using kvstore::ServerID;
using std::string;

client::client(const string& host, uint16_t port,
               read_policy::algorithm read_algo, size_t rp_num_tokens,
               uint64_t timeout_ms, uint16_t num_retries, bool print_errors)
    : clients_(),
      read_policy_(nullptr),
      num_retries_(num_retries),
      print_errors_(print_errors) {
    std::string target_str = host + ":" + std::to_string(port);
    std::shared_ptr<Channel> channel =
        grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    std::unique_ptr<ReplicatedKVStore::Stub> stub =
        ReplicatedKVStore::NewStub(channel);

    std::vector<std::tuple<int32_t, string>> srvs;
    Empty empty;
    {
        ClusterEndpoints endpoints;
        ClientContext context;
        Status status = stub->GetClusterEndpoints(&context, empty, &endpoints);

        if (!status.ok()) {
            std::cerr << "ERROR: GetClusterEndpoints RPC failed: "
                      << status.error_message() << std::endl;
        }

        for (auto& e : endpoints.endpoints()) {
            srvs.emplace_back(e.server_id(), e.client_endpoint());
        }
    }

    {
        ServerID sid;
        ClientContext context;
        Status status = stub->GetLeaderID(&context, empty, &sid);

        if (!status.ok()) {
            std::cerr << "ERROR: GetLeaderID RPC failed: "
                      << status.error_message() << std::endl;
        }

        leader_id_ = sid.server_id();
    }

    std::vector<int32_t> srv_ids;
    for (const auto& [srv_id, endpoint] : srvs) {
        std::shared_ptr<Channel> c =
            grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
        clients_.emplace(srv_id, ReplicatedKVStore::NewStub(c));
        srv_ids.push_back(srv_id);
    }

    switch (read_algo) {
        case read_policy::algorithm::round_robin:
            read_policy_ = std::make_unique<round_robin_read_policy>(srv_ids);
            break;
        case read_policy::algorithm::hash:
            read_policy_ =
                std::make_unique<hash_read_policy>(srv_ids, rp_num_tokens);
            break;
        case read_policy::algorithm::random_token:
            read_policy_ = std::make_unique<random_token_read_policy>(
                srv_ids, rp_num_tokens);
            break;
        case read_policy::algorithm::random_uniform:
            read_policy_ =
                std::make_unique<random_uniform_read_policy>(srv_ids);
            break;
        default:
            throw std::runtime_error("Invalid read policy");
    }
}

void client::trigger_cache_dumps(const string& directory) {
    // for (auto& [id, c] : clients_) {
    //     bool result = c.call(RPC_SPLINTERDB_DUMPCACHE, directory).as<bool>();

    //     if (!result) {
    //         std::cerr << "WARNING: failed to dump cache on server " << id
    //                   << std::endl;
    //     }
    // }
}

void client::trigger_cache_clear() {
    // for (auto& [id, c] : clients_) {
    //     bool result = c.call(RPC_SPLINTERDB_CLEARCACHE).as<bool>();

    //     if (!result) {
    //         std::cerr << "WARNING: failed to clear cache on server " << id
    //                   << std::endl;
    //     }
    // }
}

ReplicatedKVStore::Stub& client::get_leader_handle() {
    return *clients_.at(leader_id_);
}

bool client::try_handle_leader_change(int32_t raft_result_code) {
    if (raft_result_code == CMD_RESULT_NOT_LEADER ||
        raft_result_code == CMD_RESULT_REQUEST_CANCELLED) {
        int32_t old_leader_id = leader_id_;
        leader_id_ = get_leader_id();

        if (print_errors_) {
            std::cerr << "INFO: leader changed from " << old_leader_id << " to "
                      << leader_id_ << std::endl;
        }
        return true;
    }

    return false;
}

rpc_read_result client::get(const string& key) {
    Key req;
    req.set_key(key);
    ClientContext ctx;
    ReadResponse resp;
    Status s = clients_.find(read_policy_->next_server(key))
                   ->second->Get(&ctx, req, &resp);

    replicated_splinterdb::splinterdb_return_code kvsr;
    std::memcpy(&kvsr, resp.kvstore_result().data(), sizeof(kvsr));

    if (resp.has_value()) {
        return {resp.value(), kvsr};
    } else {
        return {std::string{}, kvsr};
    }
}

rpc_mutation_result client::retry_mutation(
    const string& key, std::function<rpc_mutation_result()> f) {
    rpc_mutation_result result;
    size_t delay_ms = 100;
    for (uint16_t i = 0; i < num_retries_; ++i) {
        result = f();

        if (was_accepted(result)) {
            break;
        } else if (get_nuraft_return_code(result) == CMD_RESULT_WEIRD_CASE) {
            std::cout << "WARNING: weird case. Verify that kvp was mutated: "
                      << key << std::endl;
            std::get<1>(result) = 0;
            break;
        } else if (try_handle_leader_change(get_nuraft_return_code(result))) {
            if (print_errors_) {
                std::cerr << "WARNING: leader changed, retrying..."
                          << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;
        }
    }

    return result;
}

rpc_mutation_result client::put(const string& key, const string& value) {
    ReplicatedKVStore::Stub& cl = get_leader_handle();
    return retry_mutation(key, [&cl, key, value]() {
        KVPair kvp;
        ClientContext ctx;
        MutationResponse mr;

        kvp.set_key(key);
        kvp.set_value(value);
        Status status = cl.Put(&ctx, kvp, &mr);

        replicated_splinterdb::splinterdb_return_code kvsr = -1;
        if (mr.raft_rc() == 0) {
            std::memcpy(&kvsr, mr.kvstore_result().data(), sizeof(kvsr));
        }

        return rpc_mutation_result{kvsr, mr.raft_rc(), mr.raft_msg()};
    });
}

rpc_mutation_result client::update(const string& key, const string& value) {
    ReplicatedKVStore::Stub& cl = get_leader_handle();
    return retry_mutation(key, [&cl, key, value]() {
        KVPair kvp;
        ClientContext ctx;
        MutationResponse mr;

        kvp.set_key(key);
        kvp.set_value(value);
        Status status = cl.Update(&ctx, kvp, &mr);

        replicated_splinterdb::splinterdb_return_code kvsr = -1;
        if (mr.raft_rc() == 0) {
            std::memcpy(&kvsr, mr.kvstore_result().data(), sizeof(kvsr));
        }

        return rpc_mutation_result{kvsr, mr.raft_rc(), mr.raft_msg()};
    });
}

rpc_mutation_result client::del(const std::string& key) {
    ReplicatedKVStore::Stub& cl = get_leader_handle();
    return retry_mutation(key, [&cl, key]() {
        Key key_msg;
        ClientContext ctx;
        MutationResponse mr;

        key_msg.set_key(key);
        Status status = cl.Delete(&ctx, key_msg, &mr);

        replicated_splinterdb::splinterdb_return_code kvsr = -1;
        if (mr.raft_rc() == 0) {
            std::memcpy(&kvsr, mr.kvstore_result().data(), sizeof(kvsr));
        }

        return rpc_mutation_result{kvsr, mr.raft_rc(), mr.raft_msg()};
    });
}

std::vector<std::tuple<int32_t, string>> client::get_all_servers() {
    for (auto& [srv_id, stub] : clients_) {
        try {
            std::vector<std::tuple<int32_t, string>> srvs;
            Empty empty;
            ClusterEndpoints endpoints;
            ClientContext context;
            Status status =
                stub->GetClusterEndpoints(&context, empty, &endpoints);

            if (!status.ok()) {
                std::cerr << "ERROR: GetClusterEndpoints RPC failed: "
                          << status.error_message() << std::endl;
            }

            for (auto& e : endpoints.endpoints()) {
                srvs.emplace_back(e.server_id(), e.client_endpoint());
            }

            return srvs;
        } catch (const std::exception& e) {
            std::cerr << "WARNING: failed to connect to " << srv_id
                      << " ... skipping. Reason:\n\t" << e.what() << std::endl;
            continue;
        }
    }

    throw std::runtime_error("failed to connect to any server");
}

int32_t client::get_leader_id() {
    size_t delay_ms = 100;
    for (auto& [srv_id, stub] : clients_) {
        try {
            for (uint16_t i = 0; i < num_retries_; ++i) {
                Empty empty;
                ServerID sid;
                ClientContext context;
                Status status = stub->GetLeaderID(&context, empty, &sid);

                if (!status.ok()) {
                    std::cerr << "ERROR: GetLeaderID RPC failed: "
                              << status.error_message() << std::endl;
                } else if (sid.server_id() != GET_LEADER_NO_LIVE_LEADER) {
                    return sid.server_id();
                } else if (print_errors_) {
                    std::cerr << "WARNING: no live leader, retrying..."
                              << std::endl;
                }

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(delay_ms));
                delay_ms *= 2;
            }

            throw std::runtime_error("no live leader");
        } catch (const std::exception& e) {
            std::cerr << "WARNING: failed to connect to " << srv_id
                      << " ... skipping. Reason: " << e.what() << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;

            continue;
        }
    }

    throw std::runtime_error("failed to connect to any server");
}

}  // namespace replicated_splinterdb