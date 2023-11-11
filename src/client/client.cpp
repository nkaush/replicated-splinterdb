#include "replicated-splinterdb/client/client.h"

#include <chrono>
#include <iostream>
#include <thread>

#include "replicated-splinterdb/common/rpc.h"

// TODOS: Implement latency-based read policy

#define GET_LEADER_NO_LIVE_LEADER (-1)
#define CMD_RESULT_NOT_LEADER (-3)
#define CMD_RESULT_REQUEST_CANCELLED (-1)
#define CMD_RESULT_WEIRD_CASE (999)

namespace replicated_splinterdb {

using std::string;

client::client(const string& host, uint16_t port,
               read_policy::algorithm read_algo, size_t rp_num_tokens,
               uint64_t timeout_ms, uint16_t num_retries, bool print_errors)
    : clients_(),
      read_policy_(nullptr),
      num_retries_(num_retries),
      print_errors_(print_errors) {
    rpc::client cl{host, port};

    std::vector<std::tuple<int32_t, string>> srvs;
    try {
        if (cl.call(RPC_PING).as<string>() != "pong") {
            throw std::runtime_error("server returned unexpected response");
        }

        srvs = cl.call(RPC_GET_ALL_SERVERS)
                   .as<std::vector<std::tuple<int32_t, string>>>();

        leader_id_ = cl.call(RPC_GET_LEADER_ID).as<int32_t>();
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        exit(1);
    }

    for (const auto& [srv_id, endpoint] : srvs) {
        auto delim_idx = endpoint.find(':');
        string srv_host = endpoint.substr(0, delim_idx);
        int srv_port = std::stoi(endpoint.substr(delim_idx + 1));

        if (1 > srv_port || srv_port > 65535) {
            string msg = "invalid port number for host \"" + srv_host +
                         "\": " + std::to_string(srv_port);
            throw std::runtime_error(msg);
        }

        auto checked_port = static_cast<uint16_t>(srv_port);
        try {
            clients_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(srv_id),
                             std::forward_as_tuple(srv_host, checked_port));
        } catch (const std::exception& e) {
            std::cerr << "WARNING: failed to connect to " << endpoint
                      << " ... skipping. Reason:\n\t" << e.what() << std::endl;
            continue;
        }
    }

    std::vector<int32_t> srv_ids;
    for (auto& [srv_id, c] : clients_) {
        c.set_timeout(static_cast<int64_t>(timeout_ms));
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
        case read_policy::algorithm::random:
            read_policy_ =
                std::make_unique<random_read_policy>(srv_ids, rp_num_tokens);
            break;
        default:
            throw std::runtime_error("Invalid read policy");
    }
}

void client::trigger_cache_dumps(const string& directory) {
    for (auto& [id, c] : clients_) {
        bool result = c.call(RPC_SPLINTERDB_DUMPCACHE, directory).as<bool>();

        if (!result) {
            std::cerr << "WARNING: failed to dump cache on server " << id
                      << std::endl;
        }
    }
}

void client::trigger_cache_clear() {
    for (auto& [id, c] : clients_) {
        bool result = c.call(RPC_SPLINTERDB_CLEARCACHE).as<bool>();
        std::cout << "moved past RPC call!" << std::endl;

        if (!result) {
            std::cerr << "WARNING: failed to clear cache on server " << id
                      << std::endl;
        }
    }
}

rpc::client& client::get_leader_handle() { return clients_.at(leader_id_); }

bool client::try_handle_leader_change(int32_t raft_rc) {
    if (raft_rc == CMD_RESULT_NOT_LEADER ||
        raft_rc == CMD_RESULT_REQUEST_CANCELLED) {
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
    return clients_.find(read_policy_->next_server(key))
        ->second.call(RPC_SPLINTERDB_GET, key)
        .as<rpc_read_result>();
}

rpc_mutation_result client::retry_mutation(
    const string& key, std::function<rpc_mutation_result()> f) {
    rpc_mutation_result result;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    return result;
}

rpc_mutation_result client::put(const string& key, const string& value) {
    rpc::client& cl = get_leader_handle();
    return retry_mutation(key, [&cl, key, value]() {
        return cl.call(RPC_SPLINTERDB_PUT, key, value)
            .as<rpc_mutation_result>();
    });
}

rpc_mutation_result client::update(const string& key, const string& value) {
    rpc::client& cl = get_leader_handle();
    return retry_mutation(key, [&cl, key, value]() {
        return cl.call(RPC_SPLINTERDB_UPDATE, key, value)
            .as<rpc_mutation_result>();
    });
}

rpc_mutation_result client::del(const std::string& key) {
    rpc::client& cl = get_leader_handle();
    return retry_mutation(key, [&cl, key]() {
        return cl.call(RPC_SPLINTERDB_DELETE, key).as<rpc_mutation_result>();
    });
}

std::vector<std::tuple<int32_t, string>> client::get_all_servers() {
    for (auto& [srv_id, c] : clients_) {
        try {
            return c.call(RPC_GET_ALL_SERVERS)
                .as<std::vector<std::tuple<int32_t, string>>>();
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
    for (auto& [srv_id, c] : clients_) {
        try {
            for (uint16_t i = 0; i < num_retries_; ++i) {
                int32_t leader_id = c.call(RPC_GET_LEADER_ID).as<int32_t>();
                if (leader_id != GET_LEADER_NO_LIVE_LEADER) {
                    return leader_id;
                }

                if (print_errors_) {
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