#ifndef REPLICATED_SPLINTERDB_CLIENT_CLIENT_H
#define REPLICATED_SPLINTERDB_CLIENT_CLIENT_H

#include <map>

#include "replicated-splinterdb/client/read_policy.h"
#include "replicated-splinterdb/common/types.h"
#include "rpc/client.h"

namespace replicated_splinterdb {

class client {
  public:
    client() = delete;

    client(const client&) = delete;

    client& operator=(const client&) = delete;

    client(const std::string& host, uint16_t port,
           read_policy::algorithm read_algo, uint64_t timeout_ms = 10000,
           uint16_t num_retries = 3, bool print_errors = false);

    rpc_read_result get(const std::string& key);

    rpc_mutation_result put(const std::string& key, const std::string& val);

    rpc_mutation_result update(const std::string& key, const std::string& val);

    rpc_mutation_result del(const std::string& key);

    void trigger_cache_dumps(const std::string& directory);

    void trigger_cache_clear();

    std::vector<std::tuple<int32_t, std::string>> get_all_servers();

    int32_t get_leader_id();

  private:
    std::map<int32_t, rpc::client> clients_;
    std::unique_ptr<read_policy> read_policy_;
    int32_t leader_id_;
    const uint16_t num_retries_;
    bool print_errors_;

    rpc::client& get_leader_handle();

    bool try_handle_leader_change(int32_t raft_result_code);
};

}  // namespace replicated_splinterdb

#endif  // REPLICATED_SPLINTERDB_CLIENT_CLIENT_H