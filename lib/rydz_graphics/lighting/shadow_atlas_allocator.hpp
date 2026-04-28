#pragma once

#include "math.hpp"
#include "types.hpp"
#include <algorithm>
#include <vector>

namespace ecs {

struct ShadowAtlasAllocator {
  struct Allocation {
    i32 x = 0;
    i32 y = 0;
    u32 size = 0;
    bool valid = false;
  };

  struct AllocationRequest {
    u32 priority = 0;
    u32 min_size = 64;
    u32 preferred_size = 512;
  };

  u32 atlas_size = 4096;
  std::vector<Allocation> allocations;

  auto reset() -> void { allocations.clear(); }

  auto allocate_batch(std::vector<AllocationRequest> const& requests)
    -> std::vector<Allocation> {
    reset();

    if (requests.empty()) {
      return {};
    }

    std::vector<usize> indices(requests.size());
    for (usize i = 0; i < indices.size(); ++i) {
      indices[i] = i;
    }
    std::ranges::sort(indices, [&](usize a, usize b) -> bool {
      return requests[a].priority > requests[b].priority;
    });

    std::vector<Allocation> results(requests.size());
    std::vector<FreeRect> free_rects;
    free_rects.push_back(
      FreeRect{.x = 0, .y = 0, .width = atlas_size, .height = atlas_size}
    );

    for (usize idx : indices) {
      auto const& req = requests[idx];

      auto alloc = try_allocate(free_rects, req.preferred_size);

      if (!alloc.valid && req.preferred_size > req.min_size) {
        u32 size = req.preferred_size / 2;
        while (size >= req.min_size && !alloc.valid) {
          alloc = try_allocate(free_rects, size);
          if (!alloc.valid) {
            size /= 2;
          }
        }
      }

      results[idx] = alloc;

      if (alloc.valid) {

        split_free_rects(free_rects, alloc);
      }
    }

    allocations = results;
    return results;
  }

private:
  struct FreeRect {
    i32 x, y;
    u32 width, height;
  };

  auto try_allocate(std::vector<FreeRect> const& free_rects, u32 size) -> Allocation {

    FreeRect const* best = nullptr;
    u32 best_area = UINT32_MAX;

    for (auto const& rect : free_rects) {
      if (rect.width >= size && rect.height >= size) {
        u32 const area = rect.width * rect.height;
        if (area < best_area) {
          best_area = area;
          best = &rect;
        }
      }
    }

    if (best == nullptr) {
      return Allocation{.valid = false};
    }

    return Allocation{
      .x = best->x,
      .y = best->y,
      .size = size,
      .valid = true,
    };
  }

  auto split_free_rects(std::vector<FreeRect>& free_rects, Allocation const& alloc)
    -> void {
    std::vector<FreeRect> new_rects;

    for (auto const& rect : free_rects) {

      if (rect.x >= alloc.x + static_cast<i32>(alloc.size) ||
          rect.x + static_cast<i32>(rect.width) <= alloc.x ||
          rect.y >= alloc.y + static_cast<i32>(alloc.size) ||
          rect.y + static_cast<i32>(rect.height) <= alloc.y) {

        new_rects.push_back(rect);
        continue;
      }

      if (rect.x < alloc.x) {
        new_rects.push_back(
          FreeRect{
            .x = rect.x,
            .y = rect.y,
            .width = static_cast<u32>(alloc.x - rect.x),
            .height = rect.height
          }
        );
      }

      i32 const alloc_right = alloc.x + static_cast<i32>(alloc.size);
      i32 const rect_right = rect.x + static_cast<i32>(rect.width);
      if (rect_right > alloc_right) {
        new_rects.push_back(
          FreeRect{
            .x = alloc_right,
            .y = rect.y,
            .width = static_cast<u32>(rect_right - alloc_right),
            .height = rect.height
          }
        );
      }

      if (rect.y < alloc.y) {
        new_rects.push_back(
          FreeRect{
            .x = rect.x,
            .y = rect.y,
            .width = rect.width,
            .height = static_cast<u32>(alloc.y - rect.y)
          }
        );
      }

      i32 const alloc_bottom = alloc.y + static_cast<i32>(alloc.size);
      i32 const rect_bottom = rect.y + static_cast<i32>(rect.height);
      if (rect_bottom > alloc_bottom) {
        new_rects.push_back(
          FreeRect{
            .x = rect.x,
            .y = alloc_bottom,
            .width = rect.width,
            .height = static_cast<u32>(rect_bottom - alloc_bottom)
          }
        );
      }
    }

    free_rects = std::move(new_rects);

    merge_free_rects(free_rects);
  }

  auto merge_free_rects(std::vector<FreeRect>& rects) -> void {

    for (usize i = 0; i < rects.size(); ++i) {
      for (usize j = i + 1; j < rects.size();) {
        if (contains(rects[i], rects[j])) {
          rects.erase(rects.begin() + static_cast<std::ptrdiff_t>(j));
        } else if (contains(rects[j], rects[i])) {
          rects[i] = rects[j];
          rects.erase(rects.begin() + static_cast<std::ptrdiff_t>(j));
        } else {
          ++j;
        }
      }
    }
  }

  static auto contains(FreeRect const& a, FreeRect const& b) -> bool {
    return b.x >= a.x && b.y >= a.y &&
           b.x + static_cast<i32>(b.width) <= a.x + static_cast<i32>(a.width) &&
           b.y + static_cast<i32>(b.height) <= a.y + static_cast<i32>(a.height);
  }
};

} // namespace ecs
