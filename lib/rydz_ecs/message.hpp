#pragma once
#include "fwd.hpp"
#include "types.hpp"
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ecs {

struct MessageId {
  usize id = 0;
};

template <typename E> class MessageRange {
  std::vector<const E *> messages_;

public:
  explicit MessageRange(std::vector<const E *> messages)
      : messages_(std::move(messages)) {}

  class iterator {
    typename std::vector<const E *>::const_iterator it_;

  public:
    explicit iterator(typename std::vector<const E *>::const_iterator iter)
        : it_(iter) {}

    auto operator*() const -> const E & { return **it_; }
    auto operator->() const -> const E * { return *it_; }

    auto operator++() -> iterator & {
      ++it_;
      return *this;
    }

    auto operator==(const iterator &other) const -> bool {
      return it_ == other.it_;
    }
    auto operator!=(const iterator &other) const -> bool {
      return it_ != other.it_;
    }
  };

  auto begin() const -> iterator { return iterator{messages_.begin()}; }
  auto end() const -> iterator { return iterator{messages_.end()}; }

  [[nodiscard]] auto empty() const -> bool { return messages_.empty(); }
  [[nodiscard]] auto size() const -> usize { return messages_.size(); }
};

template <typename E> class Messages {
public:
  using T = Resource;

private:
  using MessageInstance = std::pair<E, MessageId>;

  struct MessageBuffers {
    std::vector<MessageInstance> current;
    std::vector<MessageInstance> prev;

    auto swap() -> void { std::swap(current, prev); }
  };

  MessageBuffers buffers_;
  usize message_count_ = 0;

  std::unordered_map<usize, MessageId> reader_cursors_;
  usize next_reader_id_ = 0;

public:
  auto send(const E &message) -> void {
    MessageId id{message_count_++};
    buffers_.current.push_back({message, id});
  }

  auto send(E &&message) -> void {
    MessageId id{message_count_++};
    buffers_.current.push_back({std::move(message), id});
  }

  auto total_count() const -> usize {
    return buffers_.current.size() + buffers_.prev.size();
  }

  auto is_empty() const -> bool {
    return buffers_.current.empty() && buffers_.prev.empty();
  }

  auto update() -> void {
    buffers_.prev.clear();
    buffers_.swap();
  }

  auto clear() -> void {
    buffers_.current.clear();
    buffers_.prev.clear();
    message_count_ = 0;
  }

  auto register_reader() -> usize {
    usize idx = next_reader_id_++;
    usize start = buffers_.prev.empty() ? message_count_
                                        : buffers_.prev.front().second.id;
    reader_cursors_[idx] = MessageId{start};
    return idx;
  }

  auto iter(usize reader_id) -> MessageRange<E> {
    auto &cursor = reader_cursors_[reader_id];
    usize start_id = cursor.id;
    std::vector<const E *> unread;
    unread.reserve(total_count());

    for (auto &[message, id] : buffers_.prev) {
      if (id.id >= start_id) {
        unread.push_back(&message);
      }
    }
    for (auto &[message, id] : buffers_.current) {
      if (id.id >= start_id) {
        unread.push_back(&message);
      }
    }

    cursor.id = message_count_;
    return MessageRange<E>{std::move(unread)};
  }

  auto has_unread(usize reader_id) const -> bool {
    auto iter = reader_cursors_.find(reader_id);
    if (iter == reader_cursors_.end()) {
      return !is_empty();
    }
    return iter->second.id < message_count_;
  }
};

} // namespace ecs

#include "params.hpp"
