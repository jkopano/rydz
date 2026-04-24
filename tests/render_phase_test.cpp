#include <gtest/gtest.h>

#include "rydz_graphics/pipeline/phase.hpp"

using namespace ecs;

namespace {

auto make_command(f32 sort_key = 0.0F) -> RenderCommand {
  RenderCommand cmd{};
  cmd.sort_key = sort_key;
  return cmd;
}

TEST(RenderPhaseOpaque, ClearEmptiesItems) {
  OpaquePhase phase;
  phase.commands.push_back(make_command(1.0F));
  phase.commands.push_back(make_command(2.0F));
  ASSERT_EQ(phase.commands.size(), 2u);

  phase.clear();

  EXPECT_TRUE(phase.commands.empty());
}

TEST(RenderPhaseOpaque, ClearOnEmptyPhaseIsNoop) {
  OpaquePhase phase;
  ASSERT_TRUE(phase.commands.empty());
  phase.clear();
  EXPECT_TRUE(phase.commands.empty());
}

TEST(RenderPhaseTransparent, ClearEmptiesItems) {
  TransparentPhase phase;
  phase.commands.push_back(make_command(0.5F));
  ASSERT_EQ(phase.commands.size(), 1u);

  phase.clear();

  EXPECT_TRUE(phase.commands.empty());
}

TEST(RenderPhaseShadow, ClearEmptiesItems) {
  ShadowPhase phase;
  phase.commands.push_back(make_command());
  phase.commands.push_back(make_command());
  phase.commands.push_back(make_command());
  ASSERT_EQ(phase.commands.size(), 3u);

  phase.clear();

  EXPECT_TRUE(phase.commands.empty());
}

TEST(RenderPhaseUi, ClearEmptiesItems) {
  UiPhase phase;
  phase.items.push_back(UiPhase::Item{});
  ASSERT_EQ(phase.items.size(), 1u);

  phase.clear();

  EXPECT_TRUE(phase.items.empty());
}

TEST(RenderPhaseAliases, OpaquePhaseIsRenderPhaseOpaque) {
  OpaquePhase phase;
  phase.commands.push_back(make_command());
  phase.clear();
  EXPECT_TRUE(phase.commands.empty());
}

TEST(RenderPhaseAliases, TransparentPhaseIsRenderPhaseTransparent) {
  TransparentPhase phase;
  phase.commands.push_back(make_command());
  phase.clear();
  EXPECT_TRUE(phase.commands.empty());
}

TEST(RenderPhaseAliases, ShadowPhaseIsRenderPhaseShadow) {
  ShadowPhase phase;
  phase.commands.push_back(make_command());
  phase.clear();
  EXPECT_TRUE(phase.commands.empty());
}

TEST(RenderPhaseAliases, UiPhaseIsRenderPhaseUi) {
  UiPhase phase;
  phase.items.push_back(UiPhase::Item{});
  phase.clear();
  EXPECT_TRUE(phase.items.empty());
}

} // namespace
