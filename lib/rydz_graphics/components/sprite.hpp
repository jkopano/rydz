#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/gl/textures.hpp"

namespace ecs {

using gl::Texture;

struct Sprite {
  Handle<Texture> handle;
  Color tint = Color::WHITE;
  i32 layer{};
};

} // namespace ecs
