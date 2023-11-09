#ifndef REPLICATED_SPLINTERDB_READ_POLICY_H
#define REPLICATED_SPLINTERDB_READ_POLICY_H

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

namespace replicated_splinterdb {

class read_policy {
  public:
    enum class algorithm { hash, round_robin, random };

    read_policy(std::vector<int32_t> server_ids) : server_ids_(server_ids) {}

    virtual ~read_policy() = default;

    int32_t next_server(const std::string& k) { return server_ids_[next(k)]; }

  protected:
    virtual size_t next(const std::string& key) = 0;

    size_t num_servers() { return server_ids_.size(); }

  private:
    std::vector<int32_t> server_ids_;
};

class round_robin_read_policy : public read_policy {
  public:
    round_robin_read_policy(const std::vector<int32_t>& server_ids)
        : read_policy(server_ids), rri_(0) {}

  protected:
    size_t next(const std::string&) override {
        size_t ret = rri_;
        rri_ = (rri_ + 1) % num_servers();
        return ret;
    }

  private:
    size_t rri_;
};

class random_read_policy : public read_policy {
  public:
    random_read_policy(const std::vector<int32_t>& server_ids)
        : read_policy(server_ids) {
        srand(time(nullptr));
    }

  protected:
    size_t next(const std::string&) override { return rand() % num_servers(); }
};

class hash_read_policy : public read_policy {
  public:
    hash_read_policy(const std::vector<int32_t>& server_ids)
        : read_policy(server_ids), h_() {}

    size_t next(const std::string& k) override { return h_(k) % num_servers(); }

  private:
    std::hash<std::string> h_;
};

}  // namespace replicated_splinterdb

#endif  // REPLICATED_SPLINTERDB_READ_POLICY_H