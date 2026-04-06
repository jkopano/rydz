#pragma once

enum Direction { Row, Column };
enum struct Align { Start, Center, End };
enum struct Justify { Start, Center, End };
enum struct SizeKind { Auto, Px };

struct SizeValue {
  SizeKind kind = SizeKind::Auto;
  float value = 0.0f;

  static SizeValue auto_() { return {SizeKind::Auto, 0.0}; }
  static SizeValue px(float val) { return {SizeKind::Px, val}; }
};

struct Size2 {
  SizeValue width;
  SizeValue height;
};

struct UiRect {
  float left = 0, top = 0, right = 0, bottom = 0;
};

// defines the layout model - different layout alghorithms
// Flex - flexbox layout
// grid - grid css layout
// None - hide the node
enum Display { Flex, Grid, None };

struct Style {
  Direction direction = Direction::Row;
  Align align = Align::Start;
  Justify justify = Justify::Start;
  UiRect padding;
  UiRect margin;
  Display display = Display::Flex;
  float gap = 0.0;
  Size2 size = {{SizeKind::Auto, 0.0}, {SizeKind::Auto, 0.0}};
};
