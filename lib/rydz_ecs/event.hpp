#pragma once
#include "fwd.hpp"
#include "types.hpp"
#include <cstddef>
#include <utility>
#include <unordered_map>
#include <vector>

namespace ecs {

struct EventId {
  usize id = 0;
};

template <typename E> class Events {
public:
  using T = Resource;

private:
  using EventInstance = std::pair<E, EventId>;

  struct EventBuffers {
    std::vector<EventInstance> current;
    std::vector<EventInstance> prev;

    void swap() { std::swap(current, prev); }
  };

  EventBuffers buffers_;
  usize event_count_ = 0;

  std::unordered_map<usize, EventId> reader_cursors_;
  usize next_reader_id_ = 0;

public:
  void send(const E &event) {
    EventId id{event_count_++};
    buffers_.current.push_back({event, id});
  }

  void send(E &&event) {
    EventId id{event_count_++};
    buffers_.current.push_back({std::move(event), id});
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
    event_count_ = 0;
  }

  usize register_reader() {
    usize id = next_reader_id_++;
    usize start =
        buffers_.prev.empty() ? event_count_ : buffers_.prev.front().second.id;
    reader_cursors_[id] = EventId{start};
    return id;
  }

  template <typename Func> void read(usize reader_id, Func &&func) {
    auto &cursor = reader_cursors_[reader_id];
    usize start_id = cursor.id;

    for (auto &[evt, eid] : buffers_.prev) {
      if (eid.id >= start_id)
        func(evt);
    }
    for (auto &[evt, eid] : buffers_.current) {
      if (eid.id >= start_id)
        func(evt);
    }

    cursor.id = event_count_;
  }

  bool has_unread(usize reader_id) const {
    auto it = reader_cursors_.find(reader_id);
    if (it == reader_cursors_.end())
      return !is_empty();
    return it->second.id < event_count_;
  }
};

} // namespace ecs

#include "params.hpp"
