#ifndef REPLICATED_SPLINTERDB_SPLINTERDB_OPERATION_H
#define REPLICATED_SPLINTERDB_SPLINTERDB_OPERATION_H

#include "owned_slice.h"
#include <optional>

namespace replicated_splinterdb {

struct splinterdb_operation {
    enum splinterdb_operation_type : uint8_t {
        PUT,
        UPDATE,
        DELETE
    };

    owned_slice key_;
    std::optional<owned_slice> value_;
    splinterdb_operation_type type_;

    nuraft::ptr<nuraft::buffer> serialize() const;

    static void deserialize(nuraft::buffer& payload_in,
                            splinterdb_operation& operation_out);
};

}  // namespace replicated_splinterdb

#endif  // REPLICATED_SPLINTERDB_SPLINTERDB_OPERATION_H