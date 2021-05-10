#pragma once
// clang-format off
#include "cm_imgui_app.h"
#include "console.h"
#include "pyrepl.h"
// clang-format on
using namespace cm;

class App : public AppModule {
 public:
  // Used to access the module externally (MainModule::instance)
  static App* instance;
  ParamList   params;

  ImGuiTextBuffer lg;
  Console         console;
  bool            showConsole = true;
  float           paramWidth  = 300;
  Trigger<bool>   saveSvg_;

  bool capturing             = false;
  int  max_video_frames      = 90;
  int  captureFPS            = 30;
  int  captured_video_frames = 0;

  struct
  {
    V4 col_text;
    V4 col_main;
    V4 col_back;
    V4 col_area;
  } uiColors;

  App()
      : AppModule("Main Module"),
        params("Settings") {
    int   hue          = 140;
    float col_main_sat = 180.f / 255.f;
    float col_main_val = 161.f / 255.f;
    float col_area_sat = 124.f / 255.f;
    float col_area_val = 100.f / 255.f;
    float col_back_sat = 59.f / 255.f;
    float col_back_val = 40.f / 255.f;

    uiColors.col_text = (ImVec4)ImColor::HSV(hue / 255.f, 20.f / 255.f, 235.f / 255.f);
    uiColors.col_main = (ImVec4)ImColor::HSV(hue / 255.f, col_main_sat, col_main_val);
    uiColors.col_back = (ImVec4)ImColor::HSV(hue / 255.f, col_back_sat, col_back_val);
    uiColors.col_area = (ImVec4)ImColor::HSV(hue / 255.f, col_area_sat, col_area_val);

    instance = this;
    params.addBool("Console", &showConsole);
    params.addFloat("paramWidth", &paramWidth, 200, 600)->noGui();
    params.addEvent("Save SVG..", saveSvg_);
    params.newChild("UI Colors");
    params.addColor("text", &uiColors.col_text);
    params.addColor("main", &uiColors.col_main);
    params.addColor("back", &uiColors.col_back);
    params.addColor("area", &uiColors.col_area);
    params.newChild("Screencapture");
    params.addInt("FPS", &captureFPS);
    params.addInt("max_video_frames", &max_video_frames);

    params.loadXml(getExecutablePath() + "/settings.xml");
    std::string dt = binarize("basic_icons.ttf", "icon_font");
    printf("%s", dt.c_str());
  }

  void replError(const std::string& buf) {
    log_mutex.lock();
    console.log(buf.c_str());
    log_mutex.unlock();
  }

  void replLog(const std::string& buf) {
    //log_mutex.lock();
    console.log(buf.c_str());
    //log_mutex.unlock();
  }

  void replReload() {
    console.clear();
  }

  void startCapture() {
    std::string path;
    if (saveFileDialog(path, "mp4")) {
      gfx::beginScreenRecording(path, appWidth() - paramWidth, appHeight() - console.inputHeight, 30.);
#ifdef OSC_ENABLED
      osc.start_recording();
#endif
    }
  }

  void stopCapture() {
    gfx::endScreenRecording();
  }

  void capture() {
    if (!gfx::isScreenRecording())
      return;

    if (captured_video_frames >= max_video_frames) {
      std::cout << "Ending video capture \n";
      stopCapture();
      return;
    }
    gfx::saveScreenFrame();
    captured_video_frames++;
  }

  bool init() {
    gfx::setManualGLRelease(true);

    pyrepl::logCallback(&App::replLog, this);
    pyrepl::errCallback(&App::replError, this);
    pyrepl::reloadCallback(&App::replReload, this);
    pyrepl::resize(paramWidth, appHeight() - console.inputHeight);
    pyrepl::init();

    ui::init();
    return true;
  }

  void exit() {
    params.saveXml(getExecutablePath() + "/settings.xml");
    pyrepl::exit();
  }

  bool gui() {
    //static bool opendemo=true;
    //ImGui::ShowDemoWindow(&opendemo);
    //return false;
    // ImGui::SetupStyle(uiColors.col_text,
    //                   uiColors.col_main,
    //                   uiColors.col_back,
    //                   uiColors.col_area);
    // Resizable settings UI on the side
    static bool show = true;
    ImVec2      size = ImVec2(paramWidth, appHeight());
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowPos(ImVec2(appWidth() - paramWidth, 0));
    ImGui::Begin("COLORMOTOR", &show, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Button("<", ImVec2(5, appHeight() - 10));
    if (ImGui::IsItemActive()) {
      float dx = ImGui::GetIO().MouseDelta.x;
      paramWidth -= dx;
      if (paramWidth > 400)
        paramWidth = 400;
      if (paramWidth < 200)
        paramWidth = 200;
    }
    ImGui::SameLine();

    ImGui::BeginChild("content");
    if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {
      ///
      if (ImGui::BeginTabItem("App")) {
        imgui(params);  // Creates a UI for the parameters

        bool vis = ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_AllowItemOverlap);  //, NULL, true, true);
        if (vis) {
          ImGui::Text("Framerate: %g", pyapp::fps());
          ImGui::Text("Script: %s", pyrepl::getScriptPath().c_str());
        }

        pyrepl::gui();

        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Video recording")) {
        ImGui::InputInt("max video frames", &max_video_frames);

        {
          const char*               items[] = {"Select",
                                 "512x512",
                                 "800x800",
                                 "1024x768",
                                 "1280×720",
                                 "1920x1080"};
          const std::pair<int, int> sizes[] = {
              {512, 512},
              {800, 800},
              {1024, 768},
              {1280, 720},
              {1920, 1080}};
          static int item_current = 0;
          if (ImGui::Combo("Set Window Size",
                           &item_current,
                           items,
                           IM_ARRAYSIZE(items))) {
            if (item_current != 0) {
              cm::setWindowSize(sizes[item_current - 1].first + paramWidth,
                                sizes[item_current - 1].second + console.inputHeight);
            }
            item_current = 0;
          }
        }

        if (ImGui::Button("Save Video...")) {
          std::string path;
          if (saveFileDialog(path, "mp4")) {
            captured_video_frames = 0;
            std::cout << "Saving video " << appWidth() - paramWidth << ", " << appHeight() - console.inputHeight << std::endl;
            gfx::beginScreenRecording(path, appWidth() - paramWidth, appHeight() - console.inputHeight, 30.);
            pyrepl::reload();
          }
        }

        if (gfx::isScreenRecording()) {
          ImGui::SameLine();
          if (ImGui::Button("Stop recording")) gfx::endScreenRecording();
        }

        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    bool vis;

#ifdef OSC_ENABLED
    // OSC GOES HERE
#endif

    ImGui::EndChild();
    ImGui::End();

    log_mutex.lock();
    // Console
    if (pyrepl::hasErrors())
      console.consoleOpen = true;

    console.draw(appWidth() - paramWidth, appHeight() / 3);
    log_mutex.unlock();

    return false;
  }

  void update() {
    // Gets called every frame before render
  }

  void render() {
    float w = appWidth();
    float h = appHeight();
    if (!pyrepl::hasErrors())
      gfx::clear(0, 0, 0, 0);
    gfx::setOrtho(w, h);
    gfx::setBlendMode(gfx::BLENDMODE_ALPHA);
    gfx::color(1);
    pyrepl::resize(appWidth() - paramWidth, appHeight() - console.inputHeight);

    bool savingEps = false;
    if (saveSvg_.isTriggered()) {
      std::string path;
      if (saveFileDialog(path, "svg")) {
        gfx::beginEps(path, Box(0, 0, appWidth(), appHeight()));
        savingEps = true;
      }
    }
    pyrepl::frame();
    if (savingEps)
      gfx::endEps();

    capture();
  }
};

// allows us to access this instance externally if necessary.
#ifndef MAIN_MODULE_SINGLETON
#define MAIN_MODULE_SINGLETON
App* App::instance = 0;
#endif
