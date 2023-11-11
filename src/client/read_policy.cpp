#include "replicated-splinterdb/client/read_policy.h"

#include "MurmurHash3.h"

#define MMHSEED 1234567890U

namespace replicated_splinterdb {

int32_t read_policy::next_server(const std::string& k) {
    return server_ids_[next(k)];
}

size_t round_robin_read_policy::next(const std::string&) {
    size_t ret = rri_;
    rri_ = (rri_ + 1) % num_servers();
    return ret;
}

random_read_policy::random_read_policy(const std::vector<int32_t>& server_ids,
                                       size_t num_tokens)
    : range_based_read_policy(server_ids, num_tokens) {
    srand(time(nullptr));
}

uint32_t hash_read_policy::get_token(const std::string& k) {
    uint32_t hash;
    MurmurHash3_x86_32(k.data(), static_cast<int>(k.size()), MMHSEED, &hash);
    return hash;
}

}  // namespace replicated_splinterdb
