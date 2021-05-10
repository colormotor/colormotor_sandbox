/// Main app
/// Note: it should not be necessary to modify much here
/// except for adding your AppModule's
/// or changing the window size/name
/// Unless some more complex logic has to happen.
// clang-format off

#include "colormotor.h"

#include "app.h"
#include "cm_imgui_app.h"
// clang-format on

using namespace cm;
using namespace cv;
using namespace cv::ogl;

std::vector<AppModule*> modules;
int                     curModule = 0;

namespace cm {

int appInit(void* userData, int argc, char** argv) {
  // add your app modules here
  modules.push_back(new App());

  // initialize
  for (int i = 0; i < modules.size(); i++)
    modules[i]->init();

  return 1;
}

void appGui() {
  modules[curModule]->gui();
  //ImGui::ShowTestWindow();
  //ImGui::SetupStyleFromHue();
  //ImGui::ShowStyleEditor();
}

void appRender(float w, float h) {
  modules[curModule]->update();

  //gfx::clear(0.2,0.2,0.2,1.0);
  //gfx::enableDepthBuffer(false);
  //gfx::setOrtho(w,h);

  modules[curModule]->render();
}

void appExit() {
  for (int i = 0; i < modules.size(); i++) {
    modules[i]->exit();
    delete modules[i];
  }

  modules.clear();
}

}  // namespace cm

int main(int argc, char** argv) {
  imguiApp(argc, argv, "Colormotor Sandbox", 1280, 720, &appInit, &appExit, &appGui, &appRender);
}
