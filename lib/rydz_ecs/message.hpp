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

template <typename E> class Messages {
public:
  using T = Resource;

private:
  using MessageInstance = std::pair<E, MessageId>;

  struct MessageBuffers {
    std::vector<MessageInstance> current;
    std::vector<MessageInstance> prev;

    void swap() { std::swap(current, prev); }
  };

  MessageBuffers buffers_;
  usize message_count_ = 0;

  std::unordered_map<usize, MessageId> reader_cursors_;
  usize next_reader_id_ = 0;

public:
  void send(const E &message) {
    MessageId id{message_count_++};
    buffers_.current.push_back({message, id});
  }

  void send(E &&message) {
    MessageId id{message_count_++};
    buffers_.current.push_back({std::move(message), id});
  }

  usize total_count() const {
    return buffers_.current.size() + buffers_.prev.size();
  }

  bool is_empty() const {
    return buffers_.current.empty() && buffers_.prev.empty();
  }

  void update() {
    buffers_.prev.clear();
    buffers_.swap();
  }

  void clear() {
    buffers_.current.clear();
    buffers_.prev.clear();
    message_count_ = 0;
  }

  usize register_reader() {
    usize id = next_reader_id_++;
    usize start =
        buffers_.prev.empty() ? message_count_ : buffers_.prev.front().second.id;
    reader_cursors_[id] = MessageId{start};
    return id;
  }

  template <typename Func> void read(usize reader_id, Func &&func) {
    auto &cursor = reader_cursors_[reader_id];
    usize start_id = cursor.id;

    for (auto &[message, id] : buffers_.prev) {
      if (id.id >= start_id) {
        func(message);
      }
    }
    for (auto &[message, id] : buffers_.current) {
      if (id.id >= start_id) {
        func(message);
      }
    }

    cursor.id = message_count_;
  }

  bool has_unread(usize reader_id) const {
    auto it = reader_cursors_.find(reader_id);
    if (it == reader_cursors_.end()) {
      return !is_empty();
    }
    return it->second.id < message_count_;
  }
};

} // namespace ecs

#include "params.hpp"
