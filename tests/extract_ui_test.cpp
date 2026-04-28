#include <gtest/gtest.h>

#include "rydz_ecs/world.hpp"
#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/extract/systems.hpp"
#include "rydz_graphics/spatial/transform.hpp"

using namespace ecs;

// Helper: run Extract::ui against the world and return the result.
static auto run_extract_ui(World& world) -> ExtractedUi {
  world.insert_resource(ExtractedUi{});

  Query<Sprite, Transform> query(world, Tick{0}, world.read_change_tick());
  ResMut<ExtractedUi> ui{world.get_resource<ExtractedUi>()};

  Extract::ui(query, ui);

  return *world.get_resource<ExtractedUi>();
}

// Validates: Requirements 3.4
// Extract::ui produces the same items as the old Extract::ui +
// Extract::overlay combined (which were identical — overlay was a duplicate).
TEST(ExtractUiTest, SystemCollectsAllValidSprites) {
  World world;

  Handle<Texture> tex1{1};
  Handle<Texture> tex2{2};

  auto e1 = world.spawn();
  world.insert_component(e1, Sprite{.handle = tex1, .tint = Color::WHITE, .layer = 0});
  world.insert_component(e1, Transform{});

  auto e2 = world.spawn();
  world.insert_component(e2, Sprite{.handle = tex2, .tint = Color::RED, .layer = 1});
  world.insert_component(e2, Transform{});

  auto result = run_extract_ui(world);

  ASSERT_EQ(result.items.size(), 2u);

  bool found1 = false;
  bool found2 = false;
  for (auto const& item : result.items) {
    if (item.texture == tex1) {
      found1 = true;
      EXPECT_EQ(item.tint, Color::WHITE);
      EXPECT_EQ(item.layer, 0);
    }
    if (item.texture == tex2) {
      found2 = true;
      EXPECT_EQ(item.tint, Color::RED);
      EXPECT_EQ(item.layer, 1);
    }
  }
  EXPECT_TRUE(found1);
  EXPECT_TRUE(found2);
}

// Validates: Requirements 3.4
// Extract::ui skips sprites with invalid (null) texture handles.
TEST(ExtractUiTest, SystemSkipsInvalidHandles) {
  World world;

  auto e_invalid = world.spawn();
  world.insert_component(e_invalid, Sprite{.handle = Handle<Texture>{}, .layer = 0});
  world.insert_component(e_invalid, Transform{});

  auto e_valid = world.spawn();
  world.insert_component(e_valid, Sprite{.handle = Handle<Texture>{42}, .layer = 1});
  world.insert_component(e_valid, Transform{});

  auto result = run_extract_ui(world);

  ASSERT_EQ(result.items.size(), 1u);
  EXPECT_EQ(result.items[0].texture, (Handle<Texture>{42}));
}

// Validates: Requirements 3.4
// Calling Extract::ui twice clears and re-populates (no duplication).
TEST(ExtractUiTest, SystemClearsBeforeRepopulating) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, Sprite{.handle = Handle<Texture>{7}, .layer = 0});
  world.insert_component(e, Transform{});

  world.insert_resource(ExtractedUi{});
  Query<Sprite, Transform> query(world, Tick{0}, world.read_change_tick());
  ResMut<ExtractedUi> ui{world.get_resource<ExtractedUi>()};

  Extract::ui(query, ui);
  Extract::ui(query, ui);

  EXPECT_EQ(world.get_resource<ExtractedUi>()->items.size(), 1u);
}
