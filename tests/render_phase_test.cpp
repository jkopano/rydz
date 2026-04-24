#include <gtest/gtest.h>

#include "rydz_graphics/pipeline/phase.hpp"

using namespace ecs;

namespace {

// Helper: create a minimal DrawItem
auto make_draw_item(f32 sort_key = 0.0F) -> DrawItem {
  DrawItem item{};
  item.sort_key = sort_key;
  return item;
}

// --- RenderPhase<Opaque>::clear() ---

TEST(RenderPhaseOpaque, ClearEmptiesItems) {
  RenderPhase<Opaque> phase;
  phase.items.push_back(make_draw_item(1.0F));
  phase.items.push_back(make_draw_item(2.0F));
  ASSERT_EQ(phase.items.size(), 2u);

  phase.clear();

  EXPECT_TRUE(phase.items.empty());
}

TEST(RenderPhaseOpaque, ClearOnEmptyPhaseIsNoop) {
  RenderPhase<Opaque> phase;
  ASSERT_TRUE(phase.items.empty());
  phase.clear();
  EXPECT_TRUE(phase.items.empty());
}

// --- RenderPhase<Transparent>::clear() ---

TEST(RenderPhaseTransparent, ClearEmptiesItems) {
  RenderPhase<Transparent> phase;
  phase.items.push_back(make_draw_item(0.5F));
  ASSERT_EQ(phase.items.size(), 1u);

  phase.clear();

  EXPECT_TRUE(phase.items.empty());
}

// --- RenderPhase<Shadow>::clear() ---

TEST(RenderPhaseShadow, ClearEmptiesItems) {
  RenderPhase<Shadow> phase;
  phase.items.push_back(make_draw_item());
  phase.items.push_back(make_draw_item());
  phase.items.push_back(make_draw_item());
  ASSERT_EQ(phase.items.size(), 3u);

  phase.clear();

  EXPECT_TRUE(phase.items.empty());
}

// --- RenderPhase<Ui>::clear() ---

TEST(RenderPhaseUi, ClearEmptiesItems) {
  RenderPhase<Ui> phase;
  phase.items.push_back(make_draw_item(10.0F));
  ASSERT_EQ(phase.items.size(), 1u);

  phase.clear();

  EXPECT_TRUE(phase.items.empty());
}

// --- Type alias sanity checks ---

TEST(RenderPhaseAliases, OpaquePhaseIsRenderPhaseOpaque) {
  OpaquePhase phase;
  phase.items.push_back(make_draw_item());
  phase.clear();
  EXPECT_TRUE(phase.items.empty());
}

TEST(RenderPhaseAliases, TransparentPhaseIsRenderPhaseTransparent) {
  TransparentPhase phase;
  phase.items.push_back(make_draw_item());
  phase.clear();
  EXPECT_TRUE(phase.items.empty());
}

TEST(RenderPhaseAliases, ShadowPhaseIsRenderPhaseShadow) {
  ShadowPhase phase;
  phase.items.push_back(make_draw_item());
  phase.clear();
  EXPECT_TRUE(phase.items.empty());
}

TEST(RenderPhaseAliases, UiPhaseIsRenderPhaseUi) {
  UiPhase phase;
  phase.items.push_back(make_draw_item());
  phase.clear();
  EXPECT_TRUE(phase.items.empty());
}

} // namespace
