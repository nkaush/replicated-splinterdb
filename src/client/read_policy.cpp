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

random_read_policy::random_read_policy(const std::vector<int32_t>& server_ids)
    : read_policy(server_ids) {
    srand(time(nullptr));
}

size_t random_read_policy::next(const std::string&) {
    return rand() % num_servers();
}

hash_read_policy::hash_read_policy(const std::vector<int32_t>& server_ids)
    : read_policy(server_ids), ranges_() {
    uint32_t incr = UINT32_MAX / num_servers();
    uint32_t next = 0;
    for (uint32_t i = 0; i < num_servers(); ++i) {
        next += incr;
        ranges_.push_back(next);
    }

    ranges_.back() = UINT32_MAX;
}

size_t hash_read_policy::next(const std::string& k) {
    uint32_t hash;
    MurmurHash3_x86_32(k.data(), static_cast<int>(k.size()), MMHSEED, &hash);
    for (size_t i = 0; i < ranges_.size(); ++i) {
        if (hash <= ranges_[i]) {
            return i;
        }
    }

    return (size_t)-1;
}

}  // namespace replicated_splinterdb
