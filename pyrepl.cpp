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
#include "cm_wrap.h"
#include "app_wrap.h"

extern "C" 
{
void SWIG_init();
}

// Log function for redirecting stdout
static PyObject* captureStdout(PyObject* self, PyObject* pArgs)
{
	char* LogStr = NULL;
	if (!PyArg_ParseTuple(pArgs, "s", &LogStr)) return NULL;
	
    cm::pyrepl::log(LogStr);
	printf("> %s\n",LogStr);
	
	Py_INCREF(Py_None);
	return Py_None;
}

// Log function for redirecting stderr
static PyObject* captureStderr(PyObject* self, PyObject* pArgs)
{
	char* LogStr = NULL;
	if (!PyArg_ParseTuple(pArgs, "s", &LogStr)) return NULL;
	
	cm::pyrepl::error(LogStr);
	
	Py_INCREF(Py_None);
	return Py_None;
}

// 
static PyMethodDef logMethods[] = {
	{"CaptureStdout", captureStdout, METH_VARARGS, "Logs stdout"},
	{"CaptureStderr", captureStderr, METH_VARARGS, "Logs stderr"},
	{NULL, NULL, 0, NULL}
};


namespace cm
{

// Connects a trigger to a python func
struct PyEvent
{
    Trigger<bool> event;
    PyObject * func;
    std::string name;
};

static std::vector<PyEvent> events;

namespace pyrepl
{

static std::vector<PyEvent> events;

static int errStatus = 0;
static bool mustReload = false;
static bool scriptLoaded = false;
static bool failedToLoad = false;
static bool reloadModules = true;

static FileWatcher* watcher = 0; 

static bool active=true;
static std::string lastScript = "none";
static std::string curPath = "none";
static std::string scriptName = "";
    
// params
ParamList params("PyREPL");
ParamList scriptParams("Script Parameters");

// log and error callbacks
std::function<void(std::string)> logCb = 0;
std::function<void(std::string)> errCb = 0;
std::function<void()> reloadCb = 0;
    
// Events
static Trigger<bool> loadParams_;
static Trigger<bool> saveParams_;

static Trigger<bool> _loadScript;
static Trigger<bool> _reloadScript;
static Trigger<bool> _duplicateScript;

static PyObject* app;
    
static int renderWidth = -1;
static int renderHeight = -1;

int execute( const std::string & str )
{
	return PyRun_SimpleString(str.c_str());
}

// executes interpreter style line
int executeAndPrint( const std::string & str )
{
	std::string s = str;
	s+="\n";
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
    if (Py_FlushLine())
        PyErr_Clear();
    return 0;
}

static void print( const char *msg, ... )
{
	char msgBuf[1024];
	va_list va_alist;
	
	if (!msg) return;
	
	va_start( va_alist, msg );
	vsnprintf( msgBuf, 1024, msg, va_alist );
	va_end( va_alist );
	msgBuf[1024 - 1] = '\0';
    
    printf(msgBuf);
}

static bool hasFileChanged()
{
    if(!watcher)
        return 0;
    return watcher->hasFileChanged();
}

void log( const char *msg, ... )
{
	char msgBuf[1024];
	va_list va_alist;
	
	if (!msg) return;
	
	va_start( va_alist, msg );
	vsnprintf( msgBuf, 1024, msg, va_alist );
	va_end( va_alist );
	msgBuf[1024 - 1] = '\0';
	
    print(msgBuf);
    if(logCb)
        logCb(msgBuf);
//    for( int i = 0; i < listeners.size(); i++ )
//        listeners[i]->replLog(msgBuf);
}


void error( const char *msg, ... )
{
	char msgBuf[1024];
	va_list va_alist;
	
	if (!msg) return;
	
	va_start( va_alist, msg );
	vsnprintf( msgBuf, 1024, msg, va_alist );
	va_end( va_alist );
	msgBuf[1024 - 1] = '\0';
	
	print(msgBuf);
    //errorBuf += msgBuf;
    
    if(errCb)
        logCb(msgBuf);
//	for( int i = 0; i < listeners.size(); i++ )
//		listeners[i]->replError(msgBuf);
}
    
void dumpErrors()
{
    PyErr_Print();
}

bool saveScriptParams()
{
	std::string path;
	if( saveFileDialog(path,"xml") )
    {
		saveScriptParams(path);
        return true;
    }
	return false;
}

void saveScriptParams( const std::string & path )
{
    scriptParams.saveXml(path.c_str());
}

bool loadScriptParams()
{
	std::string path;
	if( openFileDialog(path,"xml") )
		return loadScriptParams(path);
	return false;
}

bool loadScriptParams( const std::string & path )
{
	return scriptParams.loadXml(path.c_str());
}

void exit()
{	
	std::string xm = curPath + scriptName + ".xml";
	if( curPath != "none" )
		scriptParams.saveXml(xm.c_str());
    params.saveXml(getExecutablePath() + "/repl.xml");
    
	if( watcher )
		delete watcher;

	Py_Finalize();
}

static const char * defaultClass = 
STRINGIFY(
		  class App:\n
		  \tdef init(self):\n
		  \t\tpass\n
		  \tdef exit(self):\n
		  \t\tpass\n
		  \tdef frame(self):\n
		  \t\tpass\n\n
		  
		  app = App()\n
		  );

std::string absolutPath( const std::string & path )
{
    char absPath[PATH_MAX];
    realpath(path.c_str(), absPath);
    return absPath;
}

// Call a mathod on app wrapper
bool callAppMethod( const std::string & func )
{
    PyObject *ret = PyObject_CallMethod(app, (char*)func.c_str(),NULL);
    if (ret == NULL) {
        dumpErrors();
        return false;
        //PyRun_SimpleString("sys.stderr.flush()");
    }
    
    Py_DECREF(ret);
    
    if (Py_FlushLine())
        PyErr_Clear();
    
    return true;
}

void resize( int w, int h )
{
    renderWidth = w; renderHeight = h;
}
    
bool init()
{
	gfx::setManualGLRelease(true);

	std::vector<std::string> modulePaths;

	FILE * f = fopen("./modulePaths.txt","r");
	if(f)
	{
		std::string path;
		while( readLine(path,f) )
		{
			if( path.length() > 1 )
            {
                std::string absp = absolutPath(path);
				modulePaths.push_back(absp);
            }
		}
	}
	else
	{
		printf("Could not open modulePaths.txt!\n");
	}
	
	// init gui and params
	// add default params
    params.addEvent( "load...", _loadScript );
	params.addEvent( "reload", _reloadScript )->appendOption("sameline");
    params.addEvent("duplicate...",_duplicateScript)->appendOption("sameline");;
    params.addBool( "reload modules", &reloadModules )->appendOption("sameline");
    
	params.addString( "last script",&lastScript )->appendOption("g");

	// Set program name
	Py_SetProgramName("PyREPL");
	
	// Initialize the Python interpreter.
	Py_Initialize();
	// Swig bindings
	initializeSwig_cm();
	initializeSwig_app();
    
	// Capture stderr/out
	Py_InitModule("log", logMethods);
	
	PyRun_SimpleString(
					   "import log\n"
					   "import sys\n"
					   "class StdoutCatcher:\n"
					   "\tdef write(self, str):\n"
					   "\t\tlog.CaptureStdout(str)\n"
					   "class StderrCatcher:\n"
					   "\tdef write(self, str):\n"
					   "\t\tlog.CaptureStderr(str)\n"
					   "sys.stdout = StdoutCatcher()\n"
					   "sys.stderr = StderrCatcher()\n"
					   );
	
	
	// Add current to path
	std::string pp = getCurrentDirectory();
	std::string syspath;
	syspath += "import sys\n";
    syspath += "sys.dont_write_bytecode = True\n";
	syspath += "sys.path.append(\'";
	syspath += pp;
	syspath += "\')\n\n";
	
	// add extra module paths
	for( int i = 0; i < modulePaths.size(); i++ )
	{
		syspath += "sys.path.append(\'";
		syspath += modulePaths[i];
		syspath += "\')\n";
	}
	
	printf("%s",syspath.c_str());
	PyRun_SimpleString(syspath.c_str());
    
    // startup file
    if(fileExists(getExecutablePath() + "/startup.py"))
    {
        std::string s = stringFromFile(getExecutablePath() + "/startup.py");
        PyRun_SimpleString(s.c_str());
    }
    
	// default import cm
	PyRun_SimpleString("from cm import *\n");
	PyRun_SimpleString("import app\n");
	// store old modules for full module reload
	PyRun_SimpleString("oldmods = set(sys.modules.keys())");
    
    params.loadXml(getExecutablePath() + "/repl.xml");
    if(lastScript!="none")
        mustReload = true;
    
	return true;
}

void frame()
{
	// cleanup any gfx objects that need to be deleted
	{
	//	gfxCleanup();
	}
	
	for( int i = 0; i < events.size(); i++ )
	{
		PyEvent & e = events[i];
		if(e.func && e.event.isTriggered())
		{
			log("received event %s",e.name.c_str());
			PyObject *arglist = Py_BuildValue("()");
			PyObject *result = PyEval_CallObject(e.func, arglist);
			// check for error
			if( !result )
			{
				dumpErrors();
			}
			Py_DECREF(arglist);
			
		}
	}

	if( mustReload && lastScript!="none" )
	{
		mustReload = false;
		scriptLoaded = reload();
	}
	
	// UI events
	if( _loadScript.isTriggered() )
	{
		mustReload = false;
		std::string path;
		if( openFileDialog(path,"py") )
		{
			scriptLoaded = load(path);
		}
	}
	
	if( _reloadScript.isTriggered() || hasFileChanged() )
	{
		mustReload = false;
		scriptLoaded = reload();
	}
    
    if( _duplicateScript.isTriggered() )
	{
		mustReload = false;
		scriptLoaded = duplicateScript();
	}
    
	if( loadParams_.isTriggered() )
		loadScriptParams();
	
	if( saveParams_.isTriggered() )
		saveScriptParams();
	
	if( !scriptLoaded )
		return;

	if( !active )
		return;
	
    gfx::pushViewport();
    gfx::setViewport(0, appHeight() - pyapp::height(), pyapp::width(), pyapp::height());
    gfx::setOrtho(pyapp::width(), pyapp::height());
	if(!callAppMethod("frame"))
    {
        errStatus = 1;
    }
    gfx::popViewport();
    gfx::releaseGLObjects();

}

void gui()
{
	imgui(params);
    imgui(scriptParams);
}

std::string toScriptPath( const std::string & path )
{
	return curPath+ path;
}

void onCommandEntered( const std::string & cmd )
{	
	if( cmd.length() )
		executeAndPrint(cmd);
	else
		log("");
}

static std::string readFile( const std::string & path )
{
	FILE* file = fopen(path.c_str(), "rb");
	if (file == NULL) return "error";// std::string::return v8::Handle<v8::String>();
	
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

bool load( const std::string & path, bool bInit, int reloadCount  )
{
    if(reloadCb && reloadCount==0)
        reloadCb();

    if(reloadCount==0)
    {
        errStatus = 0;
        
        if(curPath != "none")
            if(!callAppMethod("exit"))
                errStatus = 1;
    }
    
	// Save script params if load was successful
	if( curPath != "none" && reloadCount == 0 ) // Just save on successful load
	{
		std::string xm = curPath + scriptName + ".xml";
		saveScriptParams(xm.c_str());
	}
	
	// remove events
	for( int i = 0; i < events.size(); i++ )
	{
		if( events[i].func )
			Py_DECREF(events[i].func);
	}
	
	events.clear();
    
	// delete params
	scriptParams.release();
    
	if (!fileExists(path))
	{
		error("Could not load file %s",path.c_str());
		failedToLoad = true;
		return false;
	}

	std::string src = readFile(path);
	std::string prevScriptName = scriptName;
	
	// watch this script for modifications...
	if( reloadCount == 0 )
	{
		if( lastScript != path || !watcher )
		{
			SAFE_DELETE(watcher);
			watcher = new FileWatcher( path , 200 );
		}
		failedToLoad = false;
		lastScript = path;
		curPath = getDirectoryFromPath(lastScript);
		scriptName = getFilenameFromPath(lastScript);
		scriptName = getPathWithoutExt(scriptName);
	}
			
    if(reloadModules)
        PyRun_SimpleString("for mod in set(sys.modules.keys()).difference(oldmods): sys.modules.pop(mod)");
	
	PyObject* PyFileObject = PyFile_FromString((char*)path.c_str(), "r"); 
	
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
	print("%s",syspath.c_str());
	PyRun_SimpleString(syspath.c_str());
		
	PyRun_SimpleString("from cm import *\n");
	PyRun_SimpleString("import app\n");
	
	if(PyRun_SimpleFile(PyFile_AsFile(PyFileObject), path.c_str()) != 0)
	{
		log("Could not execute file %s",path.c_str());
		if( reloadCount == 0 )
		{
			failedToLoad = true;
            errStatus = 1;
            //hasError = true;
			error("Error loading script attempting to load last version");
			return load(curPath + "tmp.py",true,1);
		}
		return false;
	}
    
    PyRun_SimpleString("app.run(App())\n");
    
	log("Succesfully loaded file %s",path.c_str());

    if(!failedToLoad)
    {
        //hasError = false;
    	// Temporary file to reload on next init error
		std::string tmpPath = curPath+"tmp.py";
		FILE * f = fopen(tmpPath.c_str(),"w");
		fprintf(f,"## TMP\n%s",src.c_str());	
		fclose(f);
	}

    std::string xm = curPath+ scriptName + ".xml";
    if( curPath != "none" )
    {
        log(xm.c_str());
        loadScriptParams(xm.c_str());
    }

	// TODO Check for errors here as well
	if(bInit)
    {
		if(!callAppMethod("init"))
        {
            errStatus = 1;
        }
    }
	
	//callAppMethod("setup");
	
	return true;
}

bool reload()
{
	if( lastScript != "none" )
		return load(lastScript);
	return true;
}

// Move to CM
bool duplicateFile(const std::string & src, const std::string &dst)
{
    FILE * source = fopen(src.c_str(),"r");
    if(!source)
        return false;
    FILE * target = fopen(dst.c_str(),"w");
    if(!target)
        return false;
    
    int c;
    while( ( c = fgetc(source) ) != EOF )
        fputc(c, target);
    
    fclose(source);
    fclose(target);
    return true;
}
        
    
bool duplicateScript()
{
    std::string path;
    if(!saveFileDialog(path,"py"))
        return false;
    
    if(!duplicateFile(lastScript,path))
        return false;

    // save params first
    std::string xm = curPath+ scriptName + ".xml";
    saveScriptParams(xm.c_str());
    
    std::string srcXml = getPathWithoutExt(lastScript) + ".xml";
    std::string dstXml = getPathWithoutExt(path) + ".xml";
    
    duplicateFile(srcXml,dstXml);
    
    return load(path);
}


bool hasErrors()
{
    return errStatus != 0;
}

void run( PyObject * obj )
{
	if(app)
		Py_DECREF(app);

	Py_INCREF(obj);
	app = obj;
}

}


namespace pyapp
{
	double dt()
	{
        return appFrameTime();
	}
	
	double frameMsecs()
	{
        return appFrameTime() * 1000;
	}

	double fps()
	{
		return ImGui::GetIO().Framerate;
	}
	
	// app utils
	float width()
	{
        if(pyrepl::renderWidth == -1)
            return appWidth();
        return pyrepl::renderWidth;
	}

	float height()
	{
        if(pyrepl::renderHeight == -1)
            return appHeight();
        return pyrepl::renderHeight;
	}

	float mouseX() 
	{
		return Mouse::pos().x;
	}

	float mouseY() 
	{
		return Mouse::pos().y;
	}


	float mouseDX() 
	{
		return Mouse::dx();
	}

	float mouseDY() 
	{
		return Mouse::dy();
	}

	bool mouseDown( int i )
	{
        return Mouse::down(i);
	}
    
    bool keyPressed( int k )
    {
        if( ui::hasFocus() )
            return false;
        return Keyboard::pressed(k,0);
    }
    
    bool keyReleased( int k )
    {
        if( ui::hasFocus() )
            return false;
        return Keyboard::released(k);
    }

    bool mouseClicked( int i) { return Mouse::clicked(i); }
    bool mouseReleased( int i ) { return Mouse::released(i); }
    bool mouseDoubeClicked( int i ) { return Mouse::doubleClicked(i); }

	void setFloat( const std::string & name, float v )
	{
		Param * p = pyrepl::scriptParams.find(name);
		// also search local exposed params....
		if( !p )
			p = pyrepl::params.find(name);
		
		if( p )
			p->setFloat(v);	
	}
				
	float getFloat( const std::string & name )
	{
		Param * p = pyrepl::scriptParams.find(name);
		// also search local exposed params....
		if( !p )
			p = pyrepl::params.find(name);
		if( p )
			return p->getFloat();
		else
		{
			pyrepl::log("could not find param %s\n",name.c_str());
			return 0;
		}
	}
		
	bool getBool( const std::string & name )
	{
		Param * p = pyrepl::scriptParams.find(name);
		// also search local exposed params....
		if( !p )
			p = pyrepl::params.find(name);
		if( p )
			return p->getBool();
		else
		{
			pyrepl::log("could not find param %s\n",name.c_str());
			return false;
		}
	}
		
	void setBool( const std::string & name, bool val )
	{
		Param * p = pyrepl::scriptParams.find(name);
		// also search local exposed params....
		if( !p )
			p = pyrepl::params.find(name);
		
		if( p )
			p->setBool(val);	
	}
		
	void addParams( const ParamList & params )
	{
		//pyrepl::scriptParams.addChild(&params);
	}


	Param* addFloat( const std::string & name, float val, float min, float max, const std::string & widgetType  )
	{
		Param * p = 0;
		p = pyrepl::scriptParams.find(name);
		if( p )
		{
			pyrepl::log("parameter %s allready there.....\n",name.c_str());
			return p;
		}
		p = pyrepl::scriptParams.addFloat(name,val,min,max);
		if( widgetType == "NUMBOX" )
		{
			p->appendOption("v");
		}
        
        return p;
	}

	void addSeparator()
	{
		pyrepl::scriptParams.addSpacer();
	}
		
	Param* addBool( const std::string & name, bool val )
	{
		Param * p = 0;
		p = pyrepl::scriptParams.find(name);
		if( p )
		{
			pyrepl::log("parameter %s allready there.....\n",name.c_str());
			return p;
		}
		p = pyrepl::scriptParams.addBool(name,val);
        return p;
	}

	Param* addString( const std::string & name, const std::string & val )
	{
		Param * p = 0;
		p = pyrepl::scriptParams.find(name);
		if( p )
		{
			pyrepl::log("parameter %s allready there.....\n",name.c_str());
			return p;
		}
		p = pyrepl::scriptParams.addString(name,0);
		p->setString(val);
        return p;
	}
	
	void setString( const std::string & name, const std::string & val )
	{
		Param * p = pyrepl::scriptParams.find(name);
		// also search local exposed params....
		if( !p )
			p = pyrepl::params.find(name);
		
		if( p )
			p->setString(val);	
	}

	std::string getString( const std::string & name )
	{
		Param * p = pyrepl::scriptParams.find(name);
		// also search local exposed params....
		if( !p )
			p = pyrepl::params.find(name);
		if( p )
			return p->getString();
		else
		{
			pyrepl::log("could not find param %s\n",name.c_str());
			return "";
		}
	}
    
    
    Param* addColor( const std::string & name, const V4 & clr )
    {
        Param * p = 0;
        p = pyrepl::scriptParams.find(name);
        if( p )
        {
            pyrepl::log("parameter %s allready there.....\n",name.c_str());
            return p;
        }
        p = pyrepl::scriptParams.addColor(name,clr);
        p->setColor(clr);
        
        return p;
    }

    V4 getColor( const std::string & name )
	{
		Param * p = pyrepl::scriptParams.find(name);
		if( !p )
			p = pyrepl::params.find(name);
		if( p )
			return p->getColor();
		else
		{
			pyrepl::log("could not find param %s\n",name.c_str());
			return V4(0,0,0,0);
		}
	}
	

	int getInt( const std::string & name )
	{
		Param * p = pyrepl::scriptParams.find(name);
		if( !p )
			p = pyrepl::params.find(name);
		if( p )
			return p->getInt();
		else
		{
			pyrepl::log("could not find param %s\n",name.c_str());
			return 0;
		}
	}

    Param* addAsyncEvent( const std::string & name, PyObject * func )
    {
        return addEvent(name, func);
    }
    
	Param* addEvent( const std::string & name, PyObject * func )
	{
		events.push_back(PyEvent());
		PyEvent & e = events.back();
		e.name = name;
		e.func = func;
		if( func )
			Py_INCREF(func);
		return pyrepl::scriptParams.addEvent(name,e.event);
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

	bool isTriggered( const std::string & name )
	{
		for (int i = 0; i < events.size(); i++)
		{
			PyEvent & e = events[i];
			if( e.name == name )
				return e.event.isTriggered();
		}
		
		pyrepl::log( "could not find event %s",name.c_str() );
		return false;
	}
	
    std::string openFolderDialog()
    {
        std::string path;
        if( cm::openFolderDialog(path,"select folder...") )
            return path;
        return "";
    }

    std::string openFileDialog( const std::string & type )
    {
        std::string path;
        if( cm::openFileDialog(path,type.c_str()) )
            return path;
        return "";
    }

    std::string saveFileDialog( const std::string & type )
    {
        std::string path;
        if( cm::saveFileDialog(path,type.c_str()) )
            return path;
        return "";
    }

    void test( const V4& cazzo ) { cazzo.print(); }
    
    void loadScript( const char * script )
    {
        pyrepl::load(pyrepl::curPath+script);
    }
		
    void foo()
    {
        printf("cazzulo\n");
    }
    
    void run(PyObject * obj)
    {
        pyrepl::run(obj);
    }
	
}

	
	
}
