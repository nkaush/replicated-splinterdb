#include "call_data.h"

#define INT_AS_BYTES(x) static_cast<char*>(static_cast<void*>(&x))

namespace replicated_splinterdb {

using grpc::Status;
using kvstore::KVStoreResult;
using kvstore::ReadResponse;
using kvstore::Value;
using std::unique_ptr;

template <class RequestType, class ReplyType>
void CallDataT<RequestType, ReplyType>::Proceed(replica& replica_instance) {
    if (status_ == CallStatus::CREATE) {
        std::cout << "C: " << this << std::endl;
        status_ = CallStatus::PROCESS;
        WaitForRequest();
    } else if (status_ == CallStatus::PROCESS) {
        AddNextToCompletionQueue(replica_instance);
        HandleRequest(replica_instance);
    } else {
        // We're done! Self-destruct!
        if (status_ != CallStatus::FINISH) {
            // Log some error message
        }
        delete this;
    }
}

void CallDataGet::HandleRequest(replica& replica_instance) {
    std::pair<unique_ptr<std::string>, int32_t> res =
        replica_instance.read(request_.key());

    if (res.first != nullptr) {
        reply_.mutable_value()->set_value(*res.first);
    }

    reply_.mutable_kvstore_result()->set_result(INT_AS_BYTES(res.second),
                                                sizeof(res.second));

    status_ = CallStatus::FINISH;
    responder_.Finish(reply_, Status::OK, this);
}

void CallDataPut::HandleRequest(replica& replica_instance) {
    // replica_instance.append_log()
}

void CallDataUpdate::HandleRequest(replica& replica_instance) {}

void CallDataDelete::HandleRequest(replica& replica_instance) {}

void CallDataGetLeaderID::HandleRequest(replica& replica_instance) {}

void CallDataGetClusterEndpoints::HandleRequest(replica& replica_instance) {}

}  // namespace replicated_splinterdb