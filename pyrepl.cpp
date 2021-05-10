/*
 *  pyrepl.cpp
 *
 *  Created by Daniel Berio on 9/9/12.
 *  http://www.enist.org
 *  Copyright 2012. All rights reserved.
 *
 */

#include "pyrepl.h"

#include "Python.h"

//#include <boost/python.hpp>
#include "app_wrap.h"
#include "cm_wrap.h"
#include "imgui_internal.h"

extern "C" {
void      SWIG_init();
PyObject* PyInit__app(void);
}

std::mutex log_mutex;

static std::wstring string2wstring(const std::string& s) {
  return std::wstring(s.begin(), s.end());
}

// this is so sick...
#define WSTR(s) (wchar_t*)string2wstring(s).c_str()

// Log function for redirecting stdout
static PyObject* captureStdout(PyObject* self, PyObject* pArgs) {
  char* LogStr = NULL;
  if (!PyArg_ParseTuple(pArgs, "s", &LogStr)) return NULL;

  //log_mutex.lock();
  cm::pyrepl::log(LogStr);
  //log_mutex.unlock();
  printf("> %s\n", LogStr);
  //log_mutex.unlock();

  Py_INCREF(Py_None);
  return Py_None;
}

// Log function for redirecting stderr
static PyObject* captureStderr(PyObject* self, PyObject* pArgs) {
  char* LogStr = NULL;
  if (!PyArg_ParseTuple(pArgs, "s", &LogStr)) return NULL;

  cm::pyrepl::errorstr(LogStr);

  Py_INCREF(Py_None);
  return Py_None;
}

// This became plain horrible
// see: https://docs.python.org/3/howto/cporting.html

struct module_state {
  PyObject* error;
};

#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))

static int myextension_traverse(PyObject* m, visitproc visit, void* arg) {
  Py_VISIT(GETSTATE(m)->error);
  return 0;
}

static int myextension_clear(PyObject* m) {
  Py_CLEAR(GETSTATE(m)->error);
  return 0;
}

//
static PyMethodDef logMethods[] = {
    {"CaptureStdout", captureStdout, METH_VARARGS, "Logs stdout"},
    {"CaptureStderr", captureStderr, METH_VARARGS, "Logs stderr"},
    {NULL, NULL, 0, NULL}};

// Capture stderr/out
// http://python3porting.com/cextensions.html
// see also
// https://stackoverflow.com/questions/22569833/migrating-from-python-2-to-python-3-embedding-issues
static struct PyModuleDef log_moduledef = {
    PyModuleDef_HEAD_INIT,
    "log",
    "stdout, stderr capture",
    -1,
    logMethods,
    NULL,
    NULL,
    NULL,
    NULL};

static PyObject* PyInit__log(void) {
  return PyModule_Create(&log_moduledef);
}

//using namespace cm;
namespace cm {

// Connects a trigger to a python func
struct PyEvent {
  Trigger<bool> event;
  PyObject*     func;
  std::string   name;
};

namespace pyrepl {

static std::string app_desc = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

static std::vector<PyEvent*> events;

static int  errStatus         = 0;
static bool mustReload        = false;
static bool scriptLoaded      = false;
static bool failedToLoad      = false;
static bool reloadModules     = true;
static bool bLoadScriptParams = true;

static FileWatcher* watcher = 0;

static bool        active     = true;
static std::string lastScript = "none";
static std::string curPath    = "none";
static std::string scriptName = "";

// params
ParamList params("PyREPL");
ParamList scriptParams("Script Parameters");

// log and error callbacks
std::function<void(std::string)> logCb    = 0;
std::function<void(std::string)> errCb    = 0;
std::function<void()>            reloadCb = 0;

// Events
static Trigger<bool> loadParams_;
static Trigger<bool> saveParams_;

static Trigger<bool> _loadScript;
static Trigger<bool> _reloadScript;
static Trigger<bool> _duplicateScript;

static PyObject* app;

static int renderWidth  = -1;
static int renderHeight = -1;

std::string getScriptPath() { return lastScript; }
std::string getScriptName() { return scriptName; }

int execute(const std::string& str) {
  return PyRun_SimpleString(str.c_str());
}

int executef(const char* format, ...) {
  char    cmd[1000];
  va_list parameter;
  va_start(parameter, format);
  vsnprintf(cmd, 1000, format, parameter);
  va_end(parameter);
  return PyRun_SimpleString(cmd);
}

// executes interpreter style line
int executeAndPrint(const std::string& str) {
  std::string s = str;
  s += "\n";
  PyObject *m, *d, *v;
  m = PyImport_AddModule("__main__");
  if (m == NULL)
    return -1;
  d = PyModule_GetDict(m);
  v = PyRun_StringFlags(s.c_str(), Py_single_input, d, d, 0);
  if (v == NULL) {
    PyErr_Print();
    return -1;
  }
  Py_DECREF(v);
  //if (Py_FlushLine())
  PyErr_Clear();
  return 0;
}

static void print(const char* msg) {
  std::cout << msg;
}

static void fprint(const char* msg, ...) {
  char    msgBuf[1024];
  va_list va_alist;

  if (!msg) return;

  va_start(va_alist, msg);
  vsnprintf(msgBuf, 1024, msg, va_alist);
  va_end(va_alist);
  msgBuf[1024 - 1] = '\0';
  assert(strlen(msgBuf) < 1024);
  printf("%s", msgBuf);
}

static bool hasFileChanged() {
  if (!watcher)
    return 0;
  return watcher->hasFileChanged();
}

void log(const char* msg) {
  print(msg);
  if (logCb)
    logCb(msg);
  //    for( int i = 0; i < listeners.size(); i++ )
  //        listeners[i]->replLog(msgBuf);
}

void flog(const char* msg, ...) {
  char    msgBuf[1024];
  va_list va_alist;

  if (!msg) return;

  va_start(va_alist, msg);
  vsnprintf(msgBuf, 1024, msg, va_alist);
  va_end(va_alist);
  msgBuf[1024 - 1] = '\0';

  print(msgBuf);
  if (logCb)
    logCb(msgBuf);
  //    for( int i = 0; i < listeners.size(); i++ )
  //        listeners[i]->replLog(msgBuf);
}

void errorstr(const char* msg) {
  print(msg);
  //errorBuf += msgBuf;
  if (errCb)
    logCb(msg);
  //	for( int i = 0; i < listeners.size(); i++ )
  //		listeners[i]->replError(msgBuf);
}

void error(const char* msg, ...) {
#define ERR_BUF_SIZE 10000
  char    msgBuf[ERR_BUF_SIZE];
  va_list va_alist;

  if (!msg) return;

  if (strlen(msg) > ERR_BUF_SIZE - 1)
    return;

  va_start(va_alist, msg);
  vsnprintf(msgBuf, ERR_BUF_SIZE, msg, va_alist);
  va_end(va_alist);
  msgBuf[ERR_BUF_SIZE - 1] = '\0';
  print(msgBuf);
  //errorBuf += msgBuf;

  if (errCb)
    logCb(msgBuf);
  //	for( int i = 0; i < listeners.size(); i++ )
  //		listeners[i]->replError(msgBuf);
}

void dumpErrors() {
  PyErr_Print();
}

bool saveScriptParams() {
  std::string path;
  if (saveFileDialog(path, "xml")) {
    saveScriptParams(path);
    return true;
  }
  return false;
}

void saveScriptParams(const std::string& path) {
  scriptParams.saveXml(path.c_str());
}

bool loadScriptParams() {
  std::string path;
  if (openFileDialog(path, "xml"))
    return loadScriptParams(path);
  return false;
}

bool loadScriptParams(const std::string& path) {
  return scriptParams.loadXml(path.c_str());
}

void exit() {
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

#ifdef OSC_ENABLED
  PyRun_SimpleString("print \"Closing OSC server\"\n");
  PyRun_SimpleString("osc.close_server()\n");
  PyRun_SimpleString("time.sleep(1.)\n");
#endif

  PyGILState_Release(gstate);

  Py_BEGIN_ALLOW_THREADS
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
  Py_END_ALLOW_THREADS

      std::string xm = curPath + scriptName + ".xml";
  if (curPath != "none")
    scriptParams.saveXml(xm.c_str());
  params.saveXml(getExecutablePath() + "/repl.xml");

  if (watcher)
    delete watcher;

  Py_Finalize();
}

static const char* defaultClass =
    STRINGIFY(
        class App
        :\n
		  \tdef init(self)
        :\n
		  \t\tpass\n
		  \tdef exit(self)
        :\n
		  \t\tpass\n
		  \tdef frame(self)
        :\n
		  \t\tpass\n\n

             app = App()\n);

std::string absolutePath(const std::string& path) {
  char absPath[PATH_MAX];
  realpath(path.c_str(), absPath);
  return absPath;
}

// Call a mathod on app wrapper
bool callAppMethod(const std::string& func, bool optional = false) {
  if (optional) {
    if (!PyObject_HasAttrString(app, (char*)func.c_str()))
      return false;
  }

  PyObject* ret = PyObject_CallMethod(app, (char*)func.c_str(), NULL);
  if (ret != NULL) {
    Py_DECREF(ret);
    //PyRun_SimpleString("sys.stderr.flush()");
  }

  if (PyErr_Occurred()) {
    dumpErrors();
    return false;
  }

  //if (Py_FlushLine())
  PyErr_Clear();

  return true;
}

void resize(int w, int h) {
  renderWidth  = w;
  renderHeight = h;
}

std::string intToString(int v) {
  std::stringstream s;
  s << v;
  return s.str();
}

std::string floatToString(float v) {
  std::stringstream s;
  s << v;
  return s.str();
}

static std::vector<std::string> curChild;

void pushChild(const std::string& childName) {
  std::stringstream ss;
  ss << curChild.back() << "[\"" << childName << "\"]";
  curChild.push_back(ss.str());
}

void popChild() {
  assert(curChild.size() > 1);
  curChild.pop_back();
}

/// Populate a ~app.params~ dict accessible to the scripts
void addParamsToDict(ParamList* params) {
  static const char* bools[2] = {"False", "True"};

  for (int i = 0; i < params->getNumParams(); i++) {
    Param* p = params->getParam(i);
    switch (p->getType()) {
      case PARAM_BOOL: {
        executef("%s[\"%s\"]=%s", curChild.back().c_str(), p->getName(), bools[(int)p->getBool()]);
        break;
      }

      case PARAM_INT:
      case PARAM_SELECTION: {
        executef("%s[\"%s\"]=%s", curChild.back().c_str(), p->getName(), intToString(p->getInt()).c_str());
        break;
      }

      case PARAM_FLOAT:
      case PARAM_DOUBLE: {
        executef("%s[\"%s\"]=%s", curChild.back().c_str(), p->getName(), floatToString(p->getFloat()).c_str());
        break;
      }

      case PARAM_STRING: {
        executef("%s[\"%s\"]=\"%s\"", curChild.back().c_str(), p->getName(), p->getString());
        break;
      }

      case PARAM_COLOR: {
        V4 clr = p->getColor();
        executef("%s[\"%s\"]=np.array([%g, %g, %g, %g])", curChild.back().c_str(), p->getName(), clr.x, clr.y, clr.z, clr.w);
      }

      default:
        break;
    }
  }

  for (int i = 0; i < params->getNumChildren(); i++) {
    ParamList*  child     = params->getChild(i);
    std::string childName = child->name;
    executef("%s[\"%s\"]={}", curChild.back().c_str(), childName.c_str());
    pushChild(childName);
    addParamsToDict(child);
    popChild();
  }
}

void setupParamDict() {
  PyRun_SimpleString("app.params={}");

  // add first entry to child stack
  curChild.clear();
  curChild.push_back("app.params");
  addParamsToDict(&scriptParams);
}

bool init() {
  // Useful to know we are running interactive from custom packages
  putenv("COLORMOTOR_GL=1");

  gfx::setManualGLRelease(true);

  std::vector<std::string> modulePaths;

  printf("********* Reading colormotor base path\n");
  {
    char* cmpath = getenv("COLORMOTOR_PATH");
    if (!cmpath) {
      printf("Could not read COLORMOTOR_PATH\n");
      printf("Assuming: ./../../colormotor/addons/pycolormotor/modules");
      modulePaths.push_back("./../../colormotor/addons/pycolormotor/modules");
    } else {
      modulePaths.push_back(std::string(cmpath) +
                            "/addons/pycolormotor/modules");
    }
  }

  printf("Setting Module Paths\n");

  FILE* f = fopen("./modulePaths.txt", "r");
  if (f) {
    std::string path;
    while (readLine(path, f)) {
      if (path.length() > 1) {
        std::string absp = absolutePath(path);
        modulePaths.push_back(absp);
      }
    }
  } else {
    printf("Could not open modulePaths.txt!\n");
  }

  // init gui and params
  // add default params
  params.addEvent("load...", _loadScript);
  params.addEvent("reload", _reloadScript)->appendOption("sameline");
  params.addEvent("duplicate...", _duplicateScript)->appendOption("sameline");
  params.addBool("reload modules", &reloadModules);
  params.addBool("load params", &bLoadScriptParams)->appendOption("sameline");
  params.addString("last script", &lastScript)->appendOption("g");

  // Set program name
  Py_SetProgramName(WSTR("colormotor_sandbox"));
#ifdef PYTHON_HOME
  Py_SetPythonHome(WSTR(PYTHON_HOME));
#endif

  std::wcout << L"Initializing Python\n";
  std::wcout << L"Python home: " << Py_GetPythonHome() << std::endl;
  std::wcout << L"Python path: " << Py_GetPath() << std::endl;
  std::wcout << L"Python exec path: " << Py_GetProgramFullPath();
  std::wcout << Py_GetPlatform() << std::endl;
  std::wcout << Py_GetCompiler() << std::endl;
  std::wcout << Py_GetBuildInfo() << std::endl;

  // Initialize the Python interpreter.
  std::cout << "Done initializing python\n";
  std::cout << "Importing swig bindings\n";
  // Swig bindings

  // ridiculous
  PyImport_AppendInittab("log", &PyInit__log);
  PyImport_AppendInittab("_app", &PyInit__app);

  //
  Py_Initialize();
  PyEval_InitThreads();

  initializeSwig_cm();
  initializeSwig_app();

  PyRun_SimpleString(
      "import log\n"
      "import sys\n"
      "class StdoutCatcher:\n"
      "\tdef write(self, str):\n"
      "\t\tlog.CaptureStdout(str)\n"
      "\tdef flush(self):\n"
      "\t\tpass\n\n"
      "class StderrCatcher:\n"
      "\tdef write(self, str):\n"
      "\t\tlog.CaptureStderr(str)\n"
      "\tdef flush(self):\n"
      "\t\tpass\n\n"
      "sys.stdout = StdoutCatcher()\n"
      "sys.stderr = StderrCatcher()\n");

  // Add current to path
  std::string pp = getCurrentDirectory();
  std::string syspath;
  syspath += "import os, sys\n";
  syspath += "sys.dont_write_bytecode = True\n";
  syspath += "sys.path.append(\'";
  syspath += pp;
  syspath += "\')\n\n";

  // add extra module paths
  for (int i = 0; i < modulePaths.size(); i++) {
    syspath += "sys.path.append(os.path.abspath(\'";
    syspath += modulePaths[i];
    syspath += "\'))\n";
  }

  printf("%s", syspath.c_str());
  PyRun_SimpleString(syspath.c_str());

  // startup file
  if (fileExists(getExecutablePath() + "/startup.py")) {
    printf("Loading startup script\n");
    std::string s = stringFromFile(getExecutablePath() + "/startup.py");
    PyRun_SimpleString(s.c_str());
  }

  // default import cm
  PyRun_SimpleString("import numpy as np");
  PyRun_SimpleString("import cm\n");
  PyRun_SimpleString("import app\n");

  // PyRun_SimpleString("from cm import *\n");
  // PyRun_SimpleString("import app\n");
  // PyRun_SimpleString("import numpy as np");
  // PyRun_SimpleString("import time");

#ifdef OSC_ENABLED
  PyRun_SimpleString("print \"Importing OSC\"\n");
  PyRun_SimpleString("import osc\n");
  PyRun_SimpleString("app.set_osc_callback = osc.server.set_callback\n");
  PyRun_SimpleString("app.set_osc_server_changed_callback = osc.server.set_server_changed_callback\n");
  PyRun_SimpleString("app.send_osc = osc.client.send\n");
  PyRun_SimpleString("print \"Initializing OSC\"\n");

  PyRun_SimpleString("osc.init_client(9999)\n");
  PyRun_SimpleString("osc.init_server(7777)\n");
  PyRun_SimpleString("osc.start_server()\n");
  PyRun_SimpleString("print \"Started OSC\"\n");
#endif

  // store old modules for full module reload
  // now in setup.py
  //PyRun_SimpleString("oldmods = set(sys.modules.keys())");

  params.loadXml(getExecutablePath() + "/repl.xml");
  if (lastScript != "none")
    mustReload = true;

  return true;
}

void frame() {
  //gfx::clear(1,1,1,1);
  gfx::setBlendMode(gfx::BLENDMODE_ALPHA);
  //return;

  // cleanup any gfx objects that need to be deleted
  {
    //	gfxCleanup();
  }

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  for (int i = 0; i < events.size(); i++) {
    PyEvent* e = events[i];
    if (e->event.val != e->event.oldVal)
      printf("Event %s: %d, %d\n", e->name.c_str(), (int)e->event.val, (int)e->event.oldVal);
    if (e->func && e->event.isTriggered()) {
      flog("received event %s", e->name.c_str());
      PyObject* arglist = Py_BuildValue("()");
      PyObject* result  = PyEval_CallObject(e->func, arglist);
      // check for error
      if (!result) {
        dumpErrors();
      }
      Py_DECREF(arglist);
    }
  }

  if (mustReload && lastScript != "none") {
    mustReload   = false;
    scriptLoaded = reload();
  }

  // UI events
  if (_loadScript.isTriggered()) {
    mustReload = false;
    std::string path;
    if (openFileDialog(path, "py")) {
      scriptLoaded = load(path);
    }
  }

  if (_reloadScript.isTriggered() || hasFileChanged()) {
    mustReload   = false;
    scriptLoaded = reload();
  }

  if (_duplicateScript.isTriggered()) {
    mustReload   = false;
    scriptLoaded = duplicateScript();
  }

  if (loadParams_.isTriggered())
    loadScriptParams();

  if (saveParams_.isTriggered())
    saveScriptParams();

  if (!scriptLoaded)
    return;

  if (!active)
    return;

  //Py_BEGIN_ALLOW_THREADS
  // Setup parameter dict
  setupParamDict();
  ImGuiIO& io  = ImGui::GetIO();
  int      dpx = io.DisplayFramebufferScale.x;
  int      dpy = io.DisplayFramebufferScale.y;
  gfx::pushViewport();
  gfx::setViewport(0, (appHeight() - pyapp::height()) * dpy, pyapp::width() * dpx, pyapp::height() * dpy);
  gfx::setOrtho(pyapp::width(), pyapp::height());
  if (!errStatus) {
    if (!callAppMethod("frame"))
      errStatus = 1;
  } else
  //	if(!callAppMethod("frame"))
  {
    //errStatus = 1;
    // Hack, make sure we are not in an IMGUI Begin/End block

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    while (ctx->CurrentWindowStack.Size > 1)
      ctx->CurrentWindowStack.pop_back();
  }
  gfx::popViewport();
  gfx::releaseGLObjects();

  PyGILState_Release(gstate);

  Py_BEGIN_ALLOW_THREADS
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  Py_END_ALLOW_THREADS
}

void gui() {
  imgui(params);
  if (app_desc.size()) {
    bool vis = ImGui::CollapsingHeader(std::string(scriptParams.getName() + " description:").c_str(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen);
    if (vis) {
      ImGui::TextWrapped(app_desc.c_str());
    }
  }

  //callAppMethod("gui", true); // optional call

  imgui(scriptParams);
}

std::string toScriptPath(const std::string& path) {
  return curPath + path;
}

void onCommandEntered(const std::string& cmd) {
  if (cmd.length())
    executeAndPrint(cmd);
  else
    log("");
}

static std::string readFile(const std::string& path) {
  FILE* file = fopen(path.c_str(), "rb");
  if (file == NULL) return "error";  // std::string::return v8::Handle<v8::String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);
  return std::string(chars);
}

bool load(const std::string& path, bool bInit, int reloadCount) {
  if (reloadCb && reloadCount == 0)
    reloadCb();

  if (reloadCount == 0) {
    errStatus = 0;

    if (curPath != "none")
      if (!callAppMethod("exit"))
        errStatus = 1;
  }

  // Clear description:
  app_desc = "";

  // Save script params if load was successful
  if (curPath != "none" && reloadCount == 0 && !failedToLoad && !errStatus)  // Just save on successful load
  {
    std::string xm = curPath + scriptName + ".xml";
    saveScriptParams(xm.c_str());
  }

  // remove events
  for (int i = 0; i < events.size(); i++) {
    if (events[i]->func)
      Py_DECREF(events[i]->func);
    delete events[i];
  }

  events.clear();

  // delete params
  scriptParams.release();

  if (!fileExists(path)) {
    error("Could not load file %s", path.c_str());
    failedToLoad = true;
    return false;
  }

  std::string src            = readFile(path);
  std::string prevScriptName = scriptName;

  // watch this script for modifications...
  if (reloadCount == 0) {
    if (lastScript != path || !watcher) {
      SAFE_DELETE(watcher);
      watcher = new FileWatcher(path, 200);
    }
    failedToLoad = false;
    lastScript   = path;
    curPath      = getDirectoryFromPath(lastScript);
    scriptName   = getFilenameFromPath(lastScript);
    scriptName   = getPathWithoutExt(scriptName);
  }

  if (reloadModules) {
    const char* cmd =
        "remods=[]\n"
        "for mod in set(sys.modules.keys()).difference(oldmods): remods.append(mod)\n"
        "for mod in remods if not 'matplotlib' in mod and not 'numpy' in mod: sys.modules.pop(mod)";
    PyRun_SimpleString("reload_modules()");  //cmd);
                                             //PyRun_SimpleString("for mod in set(sys.modules.keys()).difference(oldmods): sys.modules.pop(mod)");
  }
  //PyObject* PyFileObject = PyFile_FromString((char*)path.c_str(), "r");

  std::string chdir = "import os\n";
  chdir += "os.chdir(\'";
  chdir += curPath;
  chdir += "\')\n";
  PyRun_SimpleString(chdir.c_str());

  std::string syspath = "import sys\n";
  syspath += "if \'";
  syspath += curPath;
  syspath += "\' not in sys.path:\n";
  syspath += "\tsys.path.append(\'";
  syspath += curPath;
  syspath += "\')\n";
  fprint("%s", syspath.c_str());
  PyRun_SimpleString(syspath.c_str());

  PyRun_SimpleString("from cm import *\n");
  PyRun_SimpleString("import app\n");
  PyRun_SimpleString("import numpy as np");

  FILE* fp = fopen(path.c_str(), "r");
  if (PyRun_SimpleFile(fp, path.c_str()) != 0)  //PyFile_AsFile(PyFileObject), path.c_str()) != 0)
  {
    flog("Could not execute file %s", path.c_str());
    if (reloadCount == 0) {
      failedToLoad = true;
      errStatus    = 1;
      //hasError = true;
      error("Error loading script attempting to load last version");
      return load(curPath + "tmp.py", true, 1);
    }
    return false;
  }

  // Hack we expose a 'self' variable to access app for debugging
  PyRun_SimpleString("self = App()");
  PyRun_SimpleString("app.run(self)\n");

  flog("Succesfully loaded file %s", path.c_str());

  if (!failedToLoad) {
    //hasError = false;
    // Temporary file to reload on next init error
    std::string tmpPath = curPath + "tmp.py";
    FILE*       f       = fopen(tmpPath.c_str(), "w");
    fprintf(f, "## TMP\n%s", src.c_str());
    fclose(f);
  }

  std::string xm = curPath + scriptName + ".xml";
  if (curPath != "none" && bLoadScriptParams) {
    log(xm.c_str());
    loadScriptParams(xm.c_str());
  }

  // Setup params
  setupParamDict();

  // TODO Check for errors here as well
  if (bInit) {
    if (!callAppMethod("init")) {
      errStatus = 1;
    }
  }

  if (errStatus == 0) {
    scriptParams.setName(scriptName);
  }

  //callAppMethod("setup");

  return true;
}

bool reload() {
  if (lastScript != "none")
    return load(lastScript);
  return true;
}

// Move to CM
bool duplicateFile(const std::string& src, const std::string& dst) {
  FILE* source = fopen(src.c_str(), "r");
  if (!source)
    return false;
  FILE* target = fopen(dst.c_str(), "w");
  if (!target)
    return false;

  int c;
  while ((c = fgetc(source)) != EOF)
    fputc(c, target);

  fclose(source);
  fclose(target);
  return true;
}

bool duplicateScript() {
  std::string path;
  if (!saveFileDialog(path, "py"))
    return false;

  if (!duplicateFile(lastScript, path))
    return false;

  // save params first
  std::string xm = curPath + scriptName + ".xml";
  saveScriptParams(xm.c_str());

  std::string srcXml = getPathWithoutExt(lastScript) + ".xml";
  std::string dstXml = getPathWithoutExt(path) + ".xml";

  duplicateFile(srcXml, dstXml);

  return load(path);
}

bool hasErrors() {
  return errStatus != 0;
}

void run(PyObject* obj) {
  if (app)
    Py_DECREF(app);

  Py_INCREF(obj);
  app = obj;
}

}  // namespace pyrepl

namespace pyapp {
double dt() {
  return 0.001 * appFrameTime();
}

double frameMsecs() {
  return appFrameTime();
}

double fps() {
  return ImGui::GetIO().Framerate;
}

std::string name() {
  return pyrepl::getScriptName();
}

// app utils
float width() {
  if (pyrepl::renderWidth == -1)
    return appWidth();
  return pyrepl::renderWidth;
}

float height() {
  if (pyrepl::renderHeight == -1)
    return appHeight();
  return pyrepl::renderHeight;
}

float mouseX() {
  return Mouse::pos().x;
}

float mouseY() {
  return Mouse::pos().y;
}

float mouseDX() {
  return Mouse::dx();
}

float mouseDY() {
  return Mouse::dy();
}

V2 mouseDelta() {
  return V2(Mouse::dx(), Mouse::dy());
}

V2 mousePos() {
  return Mouse::pos();
}

V2 center() {
  return V2(width() / 2, height() / 2);
}

V2 size() {
  return V2(width(), height());
}

bool mouseDown(int i) {
  return Mouse::down(i);
}

bool keyDown(int k) {
  if (ui::hasFocus())
    return false;
  return Keyboard::down(k);
}

bool keyPressed(int k) {
  if (ui::hasFocus())
    return false;
  return Keyboard::pressed(k, 0);
}

bool keyReleased(int k) {
  if (ui::hasFocus())
    return false;
  return Keyboard::released(k);
}

bool mouseClicked(int i) { return Mouse::clicked(i); }
bool mouseReleased(int i) { return Mouse::released(i); }
bool mouseDoubeClicked(int i) { return Mouse::doubleClicked(i); }

void setFloat(const std::string& name, float v) {
  Param* p = pyrepl::scriptParams.find(name);
  // also search local exposed params....
  if (!p)
    p = pyrepl::params.find(name);

  if (p)
    p->setFloat(v);
}

float getFloat(const std::string& name) {
  Param* p = pyrepl::scriptParams.find(name);
  // also search local exposed params....
  if (!p)
    p = pyrepl::params.find(name);
  if (p)
    return p->getFloat();
  else {
    pyrepl::flog("could not find param %s\n", name.c_str());
    return 0;
  }
}

bool getBool(const std::string& name) {
  Param* p = pyrepl::scriptParams.find(name);
  // also search local exposed params....
  if (!p)
    p = pyrepl::params.find(name);
  if (p)
    return p->getBool();
  else {
    pyrepl::flog("could not find param %s\n", name.c_str());
    return false;
  }
}

void setBool(const std::string& name, bool val) {
  Param* p = pyrepl::scriptParams.find(name);
  // also search local exposed params....
  if (!p)
    p = pyrepl::params.find(name);

  if (p)
    p->setBool(val);
}

void addParams(const ParamList& params) {
  //pyrepl::scriptParams.addChild(&params);
}

Param* addFloat(const std::string& name, float val, float min, float max, const std::string& widgetType) {
  Param* p = 0;
  p        = pyrepl::scriptParams.find(name);
  if (p) {
    pyrepl::flog("parameter %s allready there.....\n", name.c_str());
    return p;
  }
  p = pyrepl::scriptParams.addFloat(name, val, min, max);
  if (widgetType == "NUMBOX") {
    p->appendOption("v");
  }

  return p;
}

Param* addInt(const std::string& name, int val) {
  Param* p = 0;
  p        = pyrepl::scriptParams.find(name);
  if (p) {
    pyrepl::flog("parameter %s allready there.....\n", name.c_str());
    return p;
  }
  p = pyrepl::scriptParams.addInt(name, val);
  return p;
}

void setInt(const std::string& name, float v) {
  Param* p = pyrepl::scriptParams.find(name);
  // also search local exposed params....
  if (!p)
    p = pyrepl::params.find(name);

  if (p)
    p->setInt(v);
}

Param* addSelection(const std::string& name, const std::vector<std::string>& selections, int val) {
  Param* p = 0;
  p        = pyrepl::scriptParams.find(name);
  if (p) {
    pyrepl::flog("parameter %s allready there.....\n", name.c_str());
    return p;
  }
  p = pyrepl::scriptParams.addSelection(name, selections, val);
  return p;
}

void addSeparator() {
  pyrepl::scriptParams.addSpacer();
}

void newChild(const std::string& childName) {
  pyrepl::scriptParams.newChild(childName);
}

Param* addBool(const std::string& name, bool val) {
  Param* p = 0;
  p        = pyrepl::scriptParams.find(name);
  if (p) {
    pyrepl::flog("parameter %s allready there.....\n", name.c_str());
    return p;
  }
  p = pyrepl::scriptParams.addBool(name, val);
  return p;
}

Param* addString(const std::string& name, const std::string& val) {
  Param* p = 0;
  p        = pyrepl::scriptParams.find(name);
  if (p) {
    pyrepl::flog("parameter %s allready there.....\n", name.c_str());
    return p;
  }
  p = pyrepl::scriptParams.addString(name, 0);
  p->setString(val);
  return p;
}

void setString(const std::string& name, const std::string& val) {
  Param* p = pyrepl::scriptParams.find(name);
  // also search local exposed params....
  if (!p)
    p = pyrepl::params.find(name);

  if (p)
    p->setString(val);
}

std::string getString(const std::string& name) {
  Param* p = pyrepl::scriptParams.find(name);
  // also search local exposed params....
  if (!p)
    p = pyrepl::params.find(name);
  if (p)
    return p->getString();
  else {
    pyrepl::flog("could not find param %s\n", name.c_str());
    return "";
  }
}

Param* addColor(const std::string& name, const V4& clr) {
  Param* p = 0;
  p        = pyrepl::scriptParams.find(name);
  if (p) {
    pyrepl::flog("parameter %s allready there.....\n", name.c_str());
    return p;
  }
  p = pyrepl::scriptParams.addColor(name, clr);
  p->setColor(clr);

  return p;
}

V4 getColor(const std::string& name) {
  Param* p = pyrepl::scriptParams.find(name);
  if (!p)
    p = pyrepl::params.find(name);
  if (p)
    return p->getColor();
  else {
    pyrepl::flog("could not find param %s\n", name.c_str());
    return V4(0, 0, 0, 0);
  }
}

int getInt(const std::string& name) {
  Param* p = pyrepl::scriptParams.find(name);
  if (!p)
    p = pyrepl::params.find(name);
  if (p)
    return p->getInt();
  else {
    pyrepl::flog("could not find param %s\n", name.c_str());
    return 0;
  }
}

Param* addAsyncEvent(const std::string& name, PyObject* func) {
  return addEvent(name, func);
}

Param* addEvent(const std::string& name, PyObject* func) {
  pyrepl::events.push_back(new PyEvent());
  PyEvent* e = pyrepl::events.back();
  e->name    = name;
  e->func    = func;
  if (func)
    Py_INCREF(func);
  return pyrepl::scriptParams.addEvent(name, e->event);
}

/*
    void setParamPath( const char * path )
    {
        // Hack, will need children instead eventually
        if(scriptParamsStack.size()>1)
            scriptParamsStack.pop_back();
        
        ParamList * child = new ParamList(path);
        childScriptParams.push_back(child);
        scriptParamsStack.push_back(child);
        pyrepl::scriptParams.addChild(child);
    }
    */

bool isTriggered(const std::string& name) {
  for (int i = 0; i < pyrepl::events.size(); i++) {
    PyEvent* e = pyrepl::events[i];
    if (e->name == name)
      return e->event.isTriggered();
  }

  pyrepl::flog("could not find event %s", name.c_str());
  return false;
}

bool isRecording() {
  return gfx::isScreenRecording();
}

std::string openFolderDialog() {
  std::string path;
  if (cm::openFolderDialog(path, "select folder..."))
    return path;
  return "";
}

std::string openFileDialog(const std::string& type) {
  std::string path;
  if (cm::openFileDialog(path, type.c_str()))
    return path;
  return "";
}

std::string saveFileDialog(const std::string& type) {
  std::string path;
  if (cm::saveFileDialog(path, type.c_str()))
    return path;
  return "";
}

void desc(const std::string& str) {
  pyrepl::app_desc = str;
}

void test(const V4& cazzo) { cazzo.print(); }

void loadScript(const char* script) {
  pyrepl::load(pyrepl::curPath + script);
}

void saveParams(const std::string& path) {
  pyrepl::saveScriptParams(path);
}

void loadParams(const std::string& path) {
  pyrepl::loadScriptParams(path);
}

void foo() {
  printf("cazzulo\n");
}

void run(PyObject* obj) {
  pyrepl::run(obj);
}

}  // namespace pyapp

}  // namespace cm
