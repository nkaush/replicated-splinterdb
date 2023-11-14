#include "replicated-splinterdb/client/read_policy.h"

#include "MurmurHash3.h"

#define MMHSEED 1234567890U

namespace replicated_splinterdb {

uint32_t hash_read_policy::get_token(const std::string& k) {
    uint32_t hash;
    MurmurHash3_x86_32(k.data(), static_cast<int>(k.size()), MMHSEED, &hash);
    return hash;
}

}  // namespace replicated_splinterdb
