/*
 *  pyrepl.h
 *
 *  Created by Daniel Berio on 9/9/12.
 *  http://www.enist.org
 *  Copyright 2012. All rights reserved.
 *
 */
#pragma once
// clang-format off
#include "colormotor.h"
#include "Python.h"
#include "cm_imgui.h"
#include "cm_params.h"

//#define OSC_ENABLED
// clang-format on
extern std::mutex log_mutex;

namespace cm {
namespace pyrepl {
std::string getScriptPath();
std::string getScriptName();

bool init();
void exit();
void update();
void gui();
void frame();

bool load(const std::string& path, bool bInit = true, int reloadCount = 0);
bool reload();
bool duplicateScript();

bool hasErrors();

void resize(int w, int h);

void log(const char* msg);
void flog(const char* msg, ...);
void error(const char* msg, ...);
void errorstr(const char* msg);
void dumpErrors();

bool saveScriptParams();
void saveScriptParams(const std::string& path);
bool loadScriptParams();
bool loadScriptParams(const std::string& path);

int  execute(const std::string& str);
int  executeAndPrint(const std::string& str);
void onCommandEntered(const std::string& cmd);

std::string toScriptPath(const std::string& path);

// std IO callback handling
extern std::function<void(std::string)> logCb;
extern std::function<void(std::string)> errCb;
extern std::function<void()>            reloadCb;

template <typename T>
void logCallback(void (T::*fp)(const std::string&), T* obj) { logCb = std::bind(fp, obj, std::placeholders::_1); }
template <typename T>
void errCallback(void (T::*fp)(const std::string&), T* obj) { errCb = std::bind(fp, obj, std::placeholders::_1); }
template <typename T>
void reloadCallback(void (T::*fp)(), T* obj) { reloadCb = std::bind(fp, obj); }

CM_INLINE void logCallback(void (*fp)(const std::string&)) { logCb = fp; }
CM_INLINE void errCallback(void (*fp)(const std::string&)) { errCb = fp; }
CM_INLINE void reloadCallback(void (*fp)()) { reloadCb = fp; }
}  // namespace pyrepl

// This is accessible to the script as the ~app~ module
namespace pyapp {
enum {
  KEY_TAB        = ImGuiKey_Tab,         // for tabbing through fields
  KEY_LEFTARROW  = ImGuiKey_LeftArrow,   // for text edit
  KEY_RIGHTARROW = ImGuiKey_RightArrow,  // for text edit
  KEY_UPARROW    = ImGuiKey_UpArrow,     // for text edit
  KEY_DOWNARROW  = ImGuiKey_DownArrow,   // for text edit
  KEY_PAGEUP     = ImGuiKey_PageUp,
  KEY_PAGEDOWN   = ImGuiKey_PageDown,
  KEY_HOME       = ImGuiKey_Home,       // for text edit
  KEY_END        = ImGuiKey_End,        // for text edit
  KEY_DELETE     = ImGuiKey_Delete,     // for text edit
  KEY_BACKSPACE  = ImGuiKey_Backspace,  // for text edit
  KEY_ENTER      = ImGuiKey_Enter,      // for text edit
  KEY_ESCAPE     = ImGuiKey_Escape,     // for text edit
  KEY_SPACE      = 32
};

double dt();
double frameMsecs();
double fps();

std::string name();

float width();
float height();
float mouseX();
float mouseY();
float mouseDX();
float mouseDY();
V2    mouseDelta();
V2    mousePos();
V2    center();
V2    size();

bool keyDown(int k);
bool keyPressed(int k);
bool keyReleased(int k);
bool mouseDown(int i);
bool mouseClicked(int i);
bool mouseReleased(int i);
bool mouseDoubeClicked(int i);

/// Used to set description of app in UI
void desc(const std::string& str);

void        test(const cm::V4& cazzo);
void        setFloat(const std::string& name, float v);
float       getFloat(const std::string& name);
bool        getBool(const std::string& name);
void        setBool(const std::string& name, bool val);
std::string getString(const std::string& name);
void        setString(const std::string& name, const std::string& val);
V4          getColor(const std::string& name);
int         getInt(const std::string& name);
void        setInt(const std::string& name, float v);

Param* addFloat(const std::string& name, float val, float min, float max, const std::string& widgetType = "SLIDER");
Param* addEvent(const std::string& name, PyObject* func = 0);
Param* addAsyncEvent(const std::string& name, PyObject* func = 0);
Param* addBool(const std::string& name, bool val);
Param* addColor(const std::string& name, const V4& clr);
Param* addString(const std::string& name, const std::string& val);
Param* addInt(const std::string& name, int val);
Param* addSelection(const std::string& name, const std::vector<std::string>& selections, int val);

void newChild(const std::string& childName);
void setParamPath(const char* path);
void addSeparator();
void addParams(const ParamList& params);

bool isTriggered(const std::string& name);
bool isRecording();

std::string openFileDialog(const std::string& type);
std::string saveFileDialog(const std::string& type);
std::string openFolderDialog();

void saveParams(const std::string& path);
void loadParams(const std::string& path);

void loadScript(const char* script);

void foo();
void run(PyObject* args);
}  // namespace pyapp
}  // namespace cm
