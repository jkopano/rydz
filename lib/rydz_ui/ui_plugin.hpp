#include "rydz_ecs/app.hpp"

enum struct UiSystemSet { Prepare, Layout, PostLayout, Render };

struct UiPlugin {
public:
  void install(ecs::App &app) {
    // app.addSystem(UiSystemSet::Prepare, prepareUiSystems)
    //     .addSystem(UiSystemSet::Layout, layoutSystem)
    //     .addSystem(UiSystemSet::PostLayout, postLayoutSystems);
  }
};
