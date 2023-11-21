#include "replicated-splinterdb/server/splinterdb_operation.h"

#include "libnuraft/buffer.hxx"

namespace replicated_splinterdb {

using nuraft::buffer;
using nuraft::buffer_serializer;
using nuraft::ptr;

ptr<buffer> splinterdb_operation::serialize_put(const std::string& key,
                                                const std::string& value) {
    size_t buffer_size = sizeof(type_) + sizeof(uint32_t) + key.size() +
                         sizeof(uint32_t) + value.size();

    ptr<buffer> buf = buffer::alloc(buffer_size);
    buffer_serializer bs(buf);

    bs.put_u8(splinterdb_operation_type::PUT);
    bs.put_str(key);
    bs.put_str(value);

    return buf;
}

nuraft::ptr<nuraft::buffer> splinterdb_operation::serialize_update(
    const std::string& key, const std::string& value) {
    size_t buffer_size = sizeof(type_) + sizeof(uint32_t) + key.size() +
                         sizeof(uint32_t) + value.size();

    ptr<buffer> buf = buffer::alloc(buffer_size);
    buffer_serializer bs(buf);

    bs.put_u8(splinterdb_operation_type::UPDATE);
    bs.put_str(key);
    bs.put_str(value);

    return buf;
}

nuraft::ptr<nuraft::buffer> splinterdb_operation::serialize_delete(
    const std::string& key) {
    size_t buffer_size = sizeof(type_) + sizeof(uint32_t) + key.size();

    ptr<buffer> buf = buffer::alloc(buffer_size);
    buffer_serializer bs(buf);

    bs.put_u8(splinterdb_operation_type::DELETE);
    bs.put_str(key);

    return buf;
}

ptr<buffer> splinterdb_operation::serialize() const {
    size_t buffer_size = sizeof(type_) + sizeof(uint32_t) + key_.size();
    if (value_.has_value()) {
        buffer_size += sizeof(uint32_t) + value_.value().size();
    }

    ptr<buffer> buf = buffer::alloc(buffer_size);
    buffer_serializer bs(buf);

    bs.put_u8(type_);
    bs.put_str(key_);
    if (value_.has_value()) {
        bs.put_str(value_.value());
    }

    return buf;
}

splinterdb_operation::splinterdb_operation(std::string&& key,
                                           std::optional<std::string>&& value,
                                           splinterdb_operation_type type)
    : key_(std::forward<std::string>(key)),
      value_(std::forward<std::optional<std::string>>(value)),
      type_(type) {}

splinterdb_operation splinterdb_operation::deserialize(buffer& payload_in) {
    buffer_serializer bs(payload_in);

    auto opty = static_cast<splinterdb_operation_type>(bs.get_u8());
    std::string key_buf = bs.get_str();

    std::optional<std::string> value_buf;
    if (opty == splinterdb_operation::PUT ||
        opty == splinterdb_operation::UPDATE) {
        value_buf = bs.get_str();
    }

    return splinterdb_operation{std::move(key_buf), std::move(value_buf), opty};
}

splinterdb_operation splinterdb_operation::make_put(std::string&& key,
                                                    std::string&& value) {
    return splinterdb_operation{std::forward<std::string>(key),
                                std::forward<std::string>(value), PUT};
}

splinterdb_operation splinterdb_operation::make_update(std::string&& key,
                                                       std::string&& value) {
    return splinterdb_operation{std::forward<std::string>(key),
                                std::forward<std::string>(value), UPDATE};
}

splinterdb_operation splinterdb_operation::make_delete(std::string&& key) {
    return splinterdb_operation{std::forward<std::string>(key), std::nullopt,
                                DELETE};
}

}  // namespace replicated_splinterdb