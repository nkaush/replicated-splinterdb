#include "call_data.h"

#include "libnuraft/buffer.hxx"

#define INT_AS_BYTES(x) static_cast<char*>(static_cast<void*>(&x))

namespace replicated_splinterdb {

using google::protobuf::Arena;
using grpc::Status;
using kvstore::KVStoreResult;
using kvstore::ReadResponse;
using nuraft::buffer;
using nuraft::ptr;
using std::unique_ptr;

template <class RequestType, class ReplyType>
void CallDataT<RequestType, ReplyType>::Proceed(replica& replica_instance) {
    if (status_ == CallStatus::CREATE) {
        status_ = CallStatus::PROCESS;
        WaitForRequest();
    } else if (status_ == CallStatus::PROCESS) {
        AddNextToCompletionQueue(replica_instance);
        HandleRequest(replica_instance);
        status_ = CallStatus::FINISH;
    } else if (status_ == CallStatus::FINISH) {
        status_ = CallStatus::CLEANUP;
        responder_.Finish(reply_, Status::OK, this);
    } else {
        // We're done! Self-destruct!
        if (status_ != CallStatus::CLEANUP) {
            throw std::runtime_error("Invalid CallData state");
        }
        delete this;
    }
}

void CallDataGet::HandleRequest(replica& replica_instance) {
    std::pair<unique_ptr<std::string>, int32_t> res =
        replica_instance.read(request_.key());

    if (res.first != nullptr) {
        reply_.set_value(*res.first);
    }

    reply_.mutable_kvstore_result()->set_result(INT_AS_BYTES(res.second),
                                                sizeof(res.second));

    AddToCompletionQueue();
}

void CallDataPut::HandleRequest(replica& replica_instance) {
    ptr<buffer> log =
        splinterdb_operation::serialize_put(request_.key(), request_.value());
    ptr<replica::raft_result> res = replica_instance.append_log(log);

    if (!res->get_accepted() || res->get_result_code() != 0) {
        reply_.mutable_repl_result()->set_rc(res->get_result_code());
        reply_.mutable_repl_result()->set_msg(res->get_result_str());
        AddToCompletionQueue();
    } else {
        res->when_ready(
            [this](replica::raft_result& res, ptr<std::exception>& exn) {
                reply_.mutable_repl_result()->set_rc(res.get_result_code());
                reply_.mutable_repl_result()->set_msg(res.get_result_str());

                int32_t rc = res.get()->get_int();
                reply_.mutable_kvstore_result()->set_result(INT_AS_BYTES(rc),
                                                            sizeof(rc));
                AddToCompletionQueue();
            });
    }
}

void CallDataUpdate::HandleRequest(replica& replica_instance) {
    ptr<buffer> log = splinterdb_operation::serialize_update(request_.key(),
                                                             request_.value());
    ptr<replica::raft_result> res = replica_instance.append_log(log);

    if (!res->get_accepted() || res->get_result_code() != 0) {
        reply_.mutable_repl_result()->set_rc(res->get_result_code());
        reply_.mutable_repl_result()->set_msg(res->get_result_str());
        AddToCompletionQueue();
    } else {
        res->when_ready(
            [this](replica::raft_result& res, ptr<std::exception>& exn) {
                reply_.mutable_repl_result()->set_rc(res.get_result_code());
                reply_.mutable_repl_result()->set_msg(res.get_result_str());

                int32_t rc = res.get()->get_int();
                reply_.mutable_kvstore_result()->set_result(INT_AS_BYTES(rc),
                                                            sizeof(rc));
                AddToCompletionQueue();
            });
    }
}

void CallDataDelete::HandleRequest(replica& replica_instance) {
    ptr<buffer> log = splinterdb_operation::serialize_delete(request_.key());
    ptr<replica::raft_result> res = replica_instance.append_log(log);

    if (!res->get_accepted() || res->get_result_code() != 0) {
        reply_.mutable_repl_result()->set_rc(res->get_result_code());
        reply_.mutable_repl_result()->set_msg(res->get_result_str());
        AddToCompletionQueue();
    } else {
        res->when_ready(
            [this](replica::raft_result& res, ptr<std::exception>& exn) {
                reply_.mutable_repl_result()->set_rc(res.get_result_code());
                reply_.mutable_repl_result()->set_msg(res.get_result_str());

                int32_t rc = res.get()->get_int();
                reply_.mutable_kvstore_result()->set_result(INT_AS_BYTES(rc),
                                                            sizeof(rc));
                AddToCompletionQueue();
            });
    }
}

void CallDataGetLeaderID::HandleRequest(replica& replica_instance) {
    reply_.set_id(replica_instance.get_leader());
    AddToCompletionQueue();
}

void CallDataGetClusterEndpoints::HandleRequest(replica& replica_instance) {
    Arena arena;
    std::vector<nuraft::ptr<nuraft::srv_config>> configs;
    replica_instance.get_all_servers(configs);

    for (auto cfg : configs) {
        kvstore::ClientFacingEndpoint* cfe =
            Arena::CreateMessage<kvstore::ClientFacingEndpoint>(&arena);

        cfe->mutable_server_id()->set_id(cfg->get_id());
        cfe->mutable_client_endpoint()->set_connection_string(cfg->get_aux());

        reply_.mutable_endpoints()->AddAllocated(cfe);
    }

    AddToCompletionQueue();
}

}  // namespace replicated_splinterdb