#ifndef REPLICATED_SPLINTERDB_TYPES_H
#define REPLICATED_SPLINTERDB_TYPES_H

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "rpc/server.h"

namespace replicated_splinterdb {

class rpc_read_result {
  public:
    rpc_read_result() = default;

    rpc_read_result(const rpc_read_result&) = delete;

    rpc_read_result& operator=(const rpc_read_result&) = delete;

    rpc_read_result(rpc_read_result&&) = default;

    rpc_read_result& operator=(rpc_read_result&&) = default;

    rpc_read_result(std::unique_ptr<std::string>&& value, int32_t rc)
        : value_(std::forward<std::unique_ptr<std::string>>(value)), rc_(rc) {}

    explicit rpc_read_result(int32_t rc)
        : value_(), rc_(rc) {}

    MSGPACK_DEFINE_ARRAY(value_, rc_);

    const std::string& value() const { return *value_; }

    int32_t rc() const { return rc_; }

  private:
    std::unique_ptr<std::string> value_;
    int32_t rc_;
};

class rpc_mutation_result {
  public:
    rpc_mutation_result() = default;

    rpc_mutation_result(const rpc_mutation_result&) = delete;

    rpc_mutation_result& operator=(const rpc_mutation_result&) = delete;

    rpc_mutation_result(rpc_mutation_result&&) = default;

    rpc_mutation_result& operator=(rpc_mutation_result&&) = default;

    rpc_mutation_result(int32_t splinterdb_rc, int32_t raft_rc,
                        const std::string& raft_msg)
        : splinterdb_rc_(splinterdb_rc),
          raft_rc_(raft_rc),
          raft_msg_(raft_msg) {}

    MSGPACK_DEFINE_ARRAY(splinterdb_rc_, raft_rc_, raft_msg_);

    bool was_accepted() const { return raft_rc_ == 0; }

    bool is_success() const { return splinterdb_rc_ == 0 && was_accepted(); }

    int32_t raft_rc() const { return raft_rc_; }

    int32_t splinterdb_rc() const { return splinterdb_rc_; }

    const std::string& raft_msg() const { return raft_msg_; }

  private:
    int32_t splinterdb_rc_;
    int32_t raft_rc_;
    std::string raft_msg_;
};

class rpc_server_info {
  public:
    rpc_server_info() = default;

    rpc_server_info(const rpc_server_info&) = delete;

    rpc_server_info& operator=(const rpc_server_info&) = delete;

    rpc_server_info(rpc_server_info&&) = default;

    rpc_server_info& operator=(rpc_server_info&&) = default;

    rpc_server_info(int32_t id, const std::string& endpoint)
        : id_(id), endpoint_(endpoint) {}

    MSGPACK_DEFINE_ARRAY(id_, endpoint_);

    int32_t id() const { return id_; }

    const std::string& endpoint() const { return endpoint_; }
  private:
    int32_t id_;
    std::string endpoint_;
};

class rpc_cluster_endpoints {
  public:
    rpc_cluster_endpoints() = default;

    rpc_cluster_endpoints(const rpc_cluster_endpoints&) = delete;

    rpc_cluster_endpoints& operator=(const rpc_cluster_endpoints&) = delete;

    rpc_cluster_endpoints(rpc_cluster_endpoints&&) = default;

    rpc_cluster_endpoints& operator=(rpc_cluster_endpoints&&) = default;

    rpc_cluster_endpoints(std::vector<rpc_server_info>&& endpoints)
        : endpoints_(std::forward<std::vector<rpc_server_info>>(endpoints)) {}

    MSGPACK_DEFINE_ARRAY(endpoints_);

    const std::vector<rpc_server_info>& endpoints() const { return endpoints_; }
  private:
    std::vector<rpc_server_info> endpoints_;
};

}  // namespace replicated_splinterdb

#endif  // REPLICATED_SPLINTERDB_TYPES_H