#ifndef REPLICATED_SPLINTERDB_SERVER_SPLINTERDB_OPERATION_H
#define REPLICATED_SPLINTERDB_SERVER_SPLINTERDB_OPERATION_H

#include <optional>

#include "libnuraft/buffer_serializer.hxx"

namespace replicated_splinterdb {

class splinterdb_operation {
  public:
    enum splinterdb_operation_type : uint8_t { PUT, UPDATE, DELETE };

    nuraft::ptr<nuraft::buffer> serialize() const;

    const std::string& key() const { return key_; }

    const std::string& value() const { return *value_; }

    splinterdb_operation_type type() const { return type_; }

    static splinterdb_operation deserialize(nuraft::buffer& payload_in);

    static splinterdb_operation make_put(std::string&& key,
                                         std::string&& value);

    static splinterdb_operation make_update(std::string&& key,
                                            std::string&& value);

    static splinterdb_operation make_delete(std::string&& key);

  private:
    splinterdb_operation(std::string&& key, std::optional<std::string>&& value,
                         splinterdb_operation_type type);

    splinterdb_operation() = delete;

    std::string key_;
    std::optional<std::string> value_;
    splinterdb_operation_type type_;
};

}  // namespace replicated_splinterdb

#endif  // REPLICATED_SPLINTERDB_SERVER_SPLINTERDB_OPERATION_H