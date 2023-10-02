#include <iostream>

#include "replica.h"
#include "in_memory_state_mgr.hxx"

namespace replicated_splinterdb {

using nuraft::asio_service;
using nuraft::buffer;
using nuraft::cs_new;
using nuraft::inmem_state_mgr;
using nuraft::ptr;
using nuraft::raft_params;
using nuraft::srv_config;

void default_raft_params_init(raft_params& params) {
    // heartbeat: 100 ms, election timeout: 200 - 400 ms.
    params.heart_beat_interval_ = 100;
    params.election_timeout_lower_bound_ = 200;
    params.election_timeout_upper_bound_ = 400;

    // Up to 5 logs will be preserved ahead the last snapshot.
    params.reserved_log_items_ = 1000000;
    // Client timeout: 3000 ms.
    params.client_req_timeout_ = 3000;
    // According to this method, `append_log` function
    // should be handled differently.
    params.return_method_ = raft_params::blocking;
}

replica::replica(const replica_config& config) 
    : server_id_(config.server_id_),
      addr_(config.addr_),
      port_(config.port_),
      endpoint_(addr_ + ":" + std::to_string(port_)),
      config_(config),
      logger_(nullptr),
      sm_(nullptr),
      smgr_(nullptr),
      launcher_(),
      raft_instance_(nullptr) {
    if (!config_.server_id_) {
        throw std::invalid_argument("server_id must be set");
    }

    std::string log_file_name = 
        config_.log_file_.value_or("./srv" + std::to_string(server_id_) + ".log");

    logger_ = cs_new<SimpleLogger>(log_file_name, config_.log_level_);
    logger_->setLogLevel(config_.log_level_);
    logger_->setDispLevel(config_.display_level_);
    logger_->setCrashDumpPath("./", true);
    logger_->start();

    sm_ = cs_new<splinterdb_state_machine>(
        config_.splinterdb_cfg_,
        config_.snapshot_frequency_ <= 0
    );

    smgr_ = cs_new<inmem_state_mgr>(server_id_, endpoint_);
}

void replica::initialize() {
    raft_params params;
    default_raft_params_init(params);
    params.snapshot_distance_ = std::max(0, config_.snapshot_frequency_);

    params.return_method_ = config_.return_method_;

    asio_service::options asio_opt;
    asio_opt.thread_pool_size_ = config_.asio_thread_pool_size_;
    asio_opt.worker_start_ = [this](uint32_t) {
        // If we enable snapshotting, need to register threads with splinterdb.
        // splinterdb_register_thread(this->sm_->get_splinterdb_handle());
    };
    asio_opt.worker_stop_ = [this](uint32_t) {
        // If we enable snapshotting, need to register threads with splinterdb.
        // splinterdb_deregister_thread(this->sm_->get_splinterdb_handle());
    };

    raft_instance_ = launcher_.init(
        sm_,
        smgr_,
        logger_,
        port_,
        asio_opt,
        params
    );

    if ( !raft_instance_ ) {
        std::cerr << "Failed to initialize launcher (see the message "
                     "in the log file)." << std::endl;
        logger_.reset();
        exit(-1);
    }

    // Wait until Raft server is ready (up to 5 seconds).
    std::cout << "initializing Raft instance ";
    for (size_t ii = 0; ii < config_.initialization_retries_; ++ii) {
        if (raft_instance_->is_initialized()) {
            std::cout << " done" << std::endl;
            return;
        }

        std::cout << ".";
        fflush(stdout);
        replicated_splinterdb::sleep_ms(config_.initialization_delay_ms_);
    }

    std::cout << " FAILED" << std::endl;
    logger_.reset();
    exit(-1);
}

void replica::shutdown(size_t time_limit_sec) {
    launcher_.shutdown(time_limit_sec);
}

std::optional<owned_slice> replica::read(slice&& key) {
    splinterdb_lookup_result result;
    splinterdb_lookup_result_init(sm_->get_splinterdb_handle(), &result, 0, NULL);

    int rc = splinterdb_lookup(
        sm_->get_splinterdb_handle(),
        std::forward<slice>(key),
        &result
    );

    if (rc) {
        return std::nullopt;
    }

    slice value;
    rc = splinterdb_lookup_result_value(&result, &value);
    if (rc) {
        return std::nullopt;
    }

    return owned_slice(value);
}


void replica::add_server(int32_t server_id, const std::string& endpoint) {
    srv_config srv_conf_to_add(server_id, endpoint);
    add_server(srv_conf_to_add);
}

void replica::add_server(const srv_config& srv_conf_to_add) {
    ptr<raft_result> ret = raft_instance_->add_srv(srv_conf_to_add);
    if (!ret->get_accepted()) {
        _s_err(logger_) << "failed to add server: " << ret->get_result_code();
        return;
    }
    
    _s_info(logger_) << "add_server succeeded [id=" << srv_conf_to_add.get_id()
                     << ", endpoint=" << srv_conf_to_add.get_endpoint() << "]";
}

void replica::append_log(const splinterdb_operation& operation, 
                         handle_commit_result handle_result) {
    ptr<buffer> new_log(operation.serialize());
    ptr<Timer> timer = cs_new<Timer>();
    ptr<raft_result> ret = raft_instance_->append_entries({new_log});

    if (!ret->get_accepted()) {
        // Log append rejected, usually because this node is not a leader.
        _s_warn(logger_) << "failed to append log: " << ret->get_result_code()
                         << " (" << usToString(timer->getTimeUs()) << ")";
        return;
    }

    if (config_.return_method_ == raft_params::blocking) {
        // Blocking mode:
        //   `append_entries` returns after getting a consensus,
        //   so that `ret` already has the result from state machine.
        ptr<std::exception> err(nullptr);
        handle_result(timer, *ret, err);
    } else if (config_.return_method_ == raft_params::async_handler) {
        // Async mode:
        //   `append_entries` returns immediately.
        //   `handle_result` will be invoked asynchronously,
        //   after getting a consensus.
        ret->when_ready( std::bind( handle_result,
                                    timer,
                                    std::placeholders::_1,
                                    std::placeholders::_2 ) );
    }
}

}  // namespace replicated_splinterdb