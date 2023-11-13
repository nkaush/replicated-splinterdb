#ifndef REPLICATED_SPLINTERDB_READ_POLICY_H
#define REPLICATED_SPLINTERDB_READ_POLICY_H

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include <iostream>

namespace replicated_splinterdb {

class read_policy {
  public:
    enum class algorithm { hash, round_robin, random };

    explicit read_policy(const std::vector<int32_t>& server_ids)
      : server_ids_(server_ids) {}

    virtual ~read_policy() = default;

    int32_t next_server(const std::string& k);

  protected:
    virtual size_t next(const std::string& key) = 0;

    size_t num_servers() { return server_ids_.size(); }

  private:
    std::vector<int32_t> server_ids_;
};

class round_robin_read_policy : public read_policy {
  public:
    explicit round_robin_read_policy(const std::vector<int32_t>& server_ids)
      : read_policy(server_ids), rri_(0) {}

  protected:
    size_t next(const std::string&) override;

  private:
    size_t rri_;
};

template <typename T>
class range_based_read_policy : public read_policy {
  public:
    range_based_read_policy(const std::vector<int32_t>& server_ids,
                            size_t num_tokens)
        : read_policy(server_ids), ranges_() {
        if (num_tokens == 0) {
            throw std::invalid_argument("num_tokens cannot be 0");
        }
        size_t total_tokens = num_servers() * num_tokens;
        T incr = std::numeric_limits<T>::max() / static_cast<T>(total_tokens);
        T next = 0;

        for (size_t i = 0; i < total_tokens; ++i) {
            next += incr;
            ranges_.push_back(next);
        }
    }

  protected:
    std::vector<T> ranges_;

    virtual T get_token(const std::string& key) = 0;

    size_t next(const std::string& key) override {
        T token = get_token(key);
        for (size_t i = 0; i < ranges_.size(); ++i) {
            if (token <= ranges_[i]) {
                return i % num_servers();
            }
        }

        return (size_t)-1;
    }
};

class random_read_policy : public range_based_read_policy<int> {
  public:
    random_read_policy(const std::vector<int32_t>& server_ids,
                       size_t num_tokens);

  protected:
    int get_token(const std::string& key) override { return rand(); }
};

class hash_read_policy : public range_based_read_policy<uint32_t> {
  public:
    hash_read_policy(const std::vector<int32_t>& server_ids, size_t num_tokens)
        : range_based_read_policy(server_ids, num_tokens) {}

  protected:
    uint32_t get_token(const std::string& key) override;
};

}  // namespace replicated_splinterdb

#endif  // REPLICATED_SPLINTERDB_READ_POLICY_H