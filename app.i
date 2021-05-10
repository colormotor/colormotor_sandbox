//(directors="1")
/* clang-format off */
%module app
    //#pragma SWIG nowarn = 322, 362
%include<typemaps.i>
%include<stl.i>
%include<std_string.i>
%include<std_vector.i>

%{
// Fuckin idiotic, mac defines "check"
#ifdef check
#undef check
#endif
#define SWIG_FILE_WITH_INIT
#include "pyrepl.h"
%}

%include "armanpy.i"
%include "wrappers/py_cm.h"

%template(StringVector) std::vector<std::string>;
%template(ConstCharVector) std::vector<const char*>;

using namespace std;

namespace cm {
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

arma::vec mouseDelta();
arma::vec mousePos();
arma::vec center();
arma::vec size();

/// Used to set description of app in UI
void desc(const std::string& str);

bool keyDown(int k);
bool keyPressed(int k);
bool keyReleased(int k);
bool mouseDown(int i);
bool mouseClicked(int i);
bool mouseReleased(int i);
bool mouseDoubeClicked(int i);

void        setFloat(const std::string& name, float v);
float       getFloat(const std::string& name);
bool        getBool(const std::string& name);
void        setBool(const std::string& name, bool val);
std::string getString(const std::string& name);
void        setString(const std::string& name, const std::string& val);
int         getInt(const std::string& name);
void        setInt(const std::string& name, float v);
arma::vec   getColor(const std::string& name);

cm::Param* addFloat(const std::string& name, float val, float min, float max, const std::string& widgetType = "SLIDER");
cm::Param* addEvent(const std::string& name, PyObject* func = 0);
cm::Param* addAsyncEvent(const std::string& name, PyObject* func = 0);
cm::Param* addBool(const std::string& name, bool val);
cm::Param* addString(const std::string& name, const std::string& val);
cm::Param* addColor(const std::string& name, const arma::vec& clr);
cm::Param* addInt(const std::string& name, int val);
cm::Param* addSelection(const std::string& name, const std::vector<std::string>& selections, int val);

void addSeparator();
void newChild(const std::string& childName);
//void addParams( const ParamList & params );

bool isTriggered(const std::string& name);
bool isRecording();

std::string openFileDialog(const std::string& type);
std::string saveFileDialog(const std::string& type);
std::string openFolderDialog();

void saveParams(const std::string& path);
void loadParams(const std::string& path);

void loadScript(const char* script);

// Automatically called and used to hook to repl loop
void run(PyObject* args);
}  // namespace pyapp
}  // namespace cm
