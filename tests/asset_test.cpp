#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// Test asset type + loader (no raylib dependency)
// ============================================================

struct TextAsset {
  std::string content;
};

class TextLoader : public AssetLoader<TextLoader, TextAsset> {
public:
  std::vector<std::string> extensions() const override { return {"txt"}; }

  TextAsset load_asset(const std::vector<uint8_t> &data,
                       const std::string & /*path*/) {
    return TextAsset{std::string(data.begin(), data.end())};
  }
};

struct NumberAsset {
  int value;
};

class NumberLoader : public AssetLoader<NumberLoader, NumberAsset> {
public:
  std::vector<std::string> extensions() const override {
    return {"num", "dat"};
  }

  NumberAsset load_asset(const std::vector<uint8_t> &data,
                         const std::string & /*path*/) {
    std::string s(data.begin(), data.end());
    return NumberAsset{std::stoi(s)};
  }
};

// ============================================================
// Tests
// ============================================================

TEST(AssetTest, HandleEquality) {
  Handle<TextAsset> a{42};
  Handle<TextAsset> b{42};
  Handle<TextAsset> c{99};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_TRUE(a.is_valid());
  EXPECT_FALSE((Handle<TextAsset>{}).is_valid());
}

TEST(AssetTest, AssetsAddAndGet) {
  Assets<TextAsset> assets;
  auto h = assets.add(TextAsset{"hello"});
  EXPECT_TRUE(assets.contains(h));
  ASSERT_NE(assets.get(h), nullptr);
  EXPECT_EQ(assets.get(h)->content, "hello");
  EXPECT_EQ(assets.count(), 1u);
}

TEST(AssetTest, AssetsSetByHandle) {
  Assets<NumberAsset> assets;
  Handle<NumberAsset> h{5};
  EXPECT_FALSE(assets.contains(h));

  assets.set(h, NumberAsset{42});
  EXPECT_TRUE(assets.contains(h));
  EXPECT_EQ(assets.get(h)->value, 42);
}

TEST(AssetTest, RegisterLoaderAndExtensions) {
  AssetServer server(".");
  server.register_loader<TextLoader>();
  server.register_loader<NumberLoader>();

  EXPECT_TRUE(server.has_loader("txt"));
  EXPECT_TRUE(server.has_loader("num"));
  EXPECT_TRUE(server.has_loader("dat"));
  EXPECT_FALSE(server.has_loader("png"));
}

TEST(AssetTest, AsyncLoadFromFile) {
  // Create temp directory and file
  std::string dir = "/tmp/ecs_asset_test";
  std::filesystem::create_directories(dir);
  {
    std::ofstream f(dir + "/hello.txt");
    f << "Hello from file!";
  }

  World world;
  world.insert_resource(Assets<TextAsset>{});

  AssetServer server(dir);
  server.register_loader<TextLoader>();

  auto handle = server.load<TextAsset>("hello.txt");
  EXPECT_TRUE(handle.is_valid());

  // Wait for async load to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Drain completed queue into Assets
  server.update(world);

  auto *assets = world.get_resource<Assets<TextAsset>>();
  ASSERT_NE(assets, nullptr);
  ASSERT_NE(assets->get(handle), nullptr);
  EXPECT_EQ(assets->get(handle)->content, "Hello from file!");

  std::filesystem::remove_all(dir);
}

TEST(AssetTest, AsyncLoadMultipleTypes) {
  std::string dir = "/tmp/ecs_asset_multi_test";
  std::filesystem::create_directories(dir);
  {
    std::ofstream f(dir + "/greeting.txt");
    f << "Hi!";
  }
  {
    std::ofstream f(dir + "/answer.num");
    f << "42";
  }

  World world;
  world.insert_resource(Assets<TextAsset>{});
  world.insert_resource(Assets<NumberAsset>{});

  AssetServer server(dir);
  server.register_loader<TextLoader>();
  server.register_loader<NumberLoader>();

  auto text_h = server.load<TextAsset>("greeting.txt");
  auto num_h = server.load<NumberAsset>("answer.num");

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  server.update(world);

  auto *text_assets = world.get_resource<Assets<TextAsset>>();
  auto *num_assets = world.get_resource<Assets<NumberAsset>>();

  ASSERT_NE(text_assets->get(text_h), nullptr);
  EXPECT_EQ(text_assets->get(text_h)->content, "Hi!");

  ASSERT_NE(num_assets->get(num_h), nullptr);
  EXPECT_EQ(num_assets->get(num_h)->value, 42);

  std::filesystem::remove_all(dir);
}

TEST(AssetTest, LoadMissingExtension) {
  AssetServer server(".");
  auto handle = server.load<TextAsset>("missing.xyz");
  EXPECT_TRUE(handle.is_valid());
}

TEST(AssetTest, LoadMissingFile) {
  AssetServer server("/tmp/nonexistent_dir_12345");
  server.register_loader<TextLoader>();

  World world;
  world.insert_resource(Assets<TextAsset>{});

  auto handle = server.load<TextAsset>("nope.txt");

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  server.update(world);

  auto *assets = world.get_resource<Assets<TextAsset>>();
  // Asset should not be loaded (file doesn't exist)
  EXPECT_EQ(assets->get(handle), nullptr);
}
