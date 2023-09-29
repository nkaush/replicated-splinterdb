#include "splinterdb_operation.h"
#include "libnuraft/buffer.hxx"

namespace replicated_splinterdb {

using nuraft::buffer_serializer;
using nuraft::buffer;
using nuraft::ptr;

ptr<buffer> splinterdb_operation::serialize() const {
    size_t buffer_size = sizeof(type_) + key_.serialized_size();
    if (value_.has_value()) {
        buffer_size += value_.value().serialized_size();
    }

    ptr<buffer> buf = buffer::alloc(buffer_size);
    buffer_serializer bs(buf);

    bs.put_u8(type_);
    key_.serialize(bs);
    if (value_.has_value()) {
        value_.value().serialize(bs);
    }

    return buf;
}

void splinterdb_operation::deserialize(buffer& payload_in, 
                                       splinterdb_operation& operation_out) {
    buffer_serializer bs(payload_in);

    operation_out.type_ = static_cast<splinterdb_operation_type>(bs.get_u8());
    owned_slice::deserialize(operation_out.key_, bs);

    if (operation_out.type_ == splinterdb_operation::PUT ||
        operation_out.type_ == splinterdb_operation::UPDATE) {
        owned_slice::deserialize(operation_out.value_.value(), bs);
    }
}

}  // namespace replicated_splinterdb