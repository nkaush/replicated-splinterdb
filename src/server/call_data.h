#pragma once

#include <grpc/support/log.h>
#include <grpcpp/alarm.h>
#include <grpcpp/grpcpp.h>

#include "kvstore.grpc.pb.h"
#include "replicated-splinterdb/server/replica.h"

namespace replicated_splinterdb {

class CallDataBase {
  public:
    CallDataBase() {}

    virtual ~CallDataBase() {}

    virtual void Proceed(replica& replica_instance) = 0;

  protected:
    virtual void WaitForRequest() = 0;

    virtual void HandleRequest(replica& replica_instance) = 0;
};

template <class RequestType, class ReplyType>
class CallDataT : CallDataBase {
  public:
    CallDataT(kvstore::ReplicatedKVStore::AsyncService* service,
              grpc::ServerCompletionQueue* cq)
        : status_(CallStatus::CREATE),
          service_(service),
          cq_(cq),
          responder_(&context_) {}

    virtual ~CallDataT() {}

  public:
    virtual void Proceed(replica& replica_instance) override;

  protected:
    enum class CallStatus { CREATE, PROCESS, FINISH, CLEANUP };
    CallStatus status_;

    kvstore::ReplicatedKVStore::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    grpc::ServerAsyncResponseWriter<ReplyType> responder_;
    grpc::ServerContext context_;
    grpc::Alarm alarm_;
    RequestType request_;
    ReplyType reply_;

    // When we handle a request of this type, we need to tell
    // the completion queue to wait for new requests of the same type.
    virtual void AddNextToCompletionQueue(replica& replica_instance) = 0;

    void AddToCompletionQueue() {
      alarm_.Set(cq_, gpr_timespec{0, 0, GPR_TIMESPAN}, this);
    }
};

#define DEFINE_RPC_CALLDATA(rpc_name, request_type, response_type)          \
    class CallData##rpc_name                                                \
        : CallDataT<kvstore::request_type, kvstore::response_type> {        \
      public:                                                               \
        CallData##rpc_name(kvstore::ReplicatedKVStore::AsyncService* svc,   \
                           grpc::ServerCompletionQueue* cq,                 \
                           replica& replica_instance)                       \
            : CallDataT(svc, cq) {                                          \
            Proceed(replica_instance);                                      \
        }                                                                   \
                                                                            \
      protected:                                                            \
        void AddNextToCompletionQueue(replica& replica_instance) override { \
            new CallData##rpc_name(service_, cq_, replica_instance);        \
        }                                                                   \
        void WaitForRequest() override {                                    \
            service_->Request##rpc_name(&context_, &request_, &responder_,  \
                                        cq_, cq_, this);                    \
        }                                                                   \
        void HandleRequest(replica& replica_instance) override;             \
    };

DEFINE_RPC_CALLDATA(Get, Key, ReadResponse);
DEFINE_RPC_CALLDATA(Put, KVPair, MutationResponse);
DEFINE_RPC_CALLDATA(Update, KVPair, MutationResponse);
DEFINE_RPC_CALLDATA(Delete, Key, MutationResponse);
DEFINE_RPC_CALLDATA(GetLeaderID, Empty, ServerID);
DEFINE_RPC_CALLDATA(GetClusterEndpoints, Empty, ClusterEndpoints);

#undef DEFINE_RPC_CALLDATA

}  // namespace replicated_splinterdb