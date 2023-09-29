#ifndef REPLICATED_SPLINTERDB_REPLICA_H
#define REPLICATED_SPLINTERDB_REPLICA_H

#include "libnuraft/nuraft.hxx"
#include "logging/logger_wrapper.hxx"
#include "replica_config.h"
#include "splinterdb_state_machine.h"
#include "splinterdb_operation.h"
#include "timer.h"

namespace replicated_splinterdb {

void default_raft_params_init(nuraft::raft_params& params);

class replica {
  public:
    using raft_result = nuraft::cmd_result<nuraft::ptr<nuraft::buffer>>;

    using handle_commit_result = 
        std::function<void(nuraft::ptr<Timer>, raft_result&, nuraft::ptr<std::exception>&)>;  

    explicit replica(const replica_config& config);

    void initialize();

    /**
     * Shutdown Raft server and ASIO service.
     * If this function is hanging even after the given timeout,
     * it will do force return.
     *
     * @param time_limit_sec Waiting timeout in seconds.
     */
    void shutdown(size_t time_limit_sec);

    void add_server(int32_t server_id, const std::string& endpoint);

    void add_server(const nuraft::srv_config& config);

    void append_log(const splinterdb_operation& operation,
                    handle_commit_result handle_result);

  private:
    int server_id_;
    std::string addr_;
    int port_;
    std::string endpoint_;

    replica_config config_;

    ptr<logger_wrapper> raft_logger_;
    ptr<splinterdb_state_machine> sm_;
    ptr<state_mgr> smgr_;
    raft_launcher launcher_;
    ptr<raft_server> raft_instance_;
};

}  // namespace replicated_splinterdb

#endif  // REPLICATED_SPLINTERDB_REPLICA_H