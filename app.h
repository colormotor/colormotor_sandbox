#pragma once
#include "colormotor.h"
#include "pyrepl.h"
#include "console.h"

using namespace cm;

class App : public AppModule
{
public:
	// Used to access the module externally (MainModule::instance)
	static App * instance;
    ParamList params;
    
    ImGuiTextBuffer lg;
    Console console;
    bool showConsole=true;
    
	App()
	:
	AppModule("Main Module")
	{
		instance = this;
        params.addBool("Console",&showConsole);
	}
    
    void replError( const std::string & buf )
    {
        console.log(buf.c_str());
    }
    
    void replLog( const std::string & buf )
    {
        console.log(buf.c_str());
    }
    
    void replReload()
    {
        console.clear();
    }
    

	bool init()
	{
        gfx::setManualGLRelease(true);
        
        pyrepl::logCallback(&App::replLog, this);
        pyrepl::errCallback(&App::replError, this);
        pyrepl::reloadCallback(&App::replReload, this);
        pyrepl::init();
        
        return true;
	}

	void exit()
	{
        pyrepl::exit();
	}

	bool gui()
	{
        imgui(params); // Creates a UI for the parameters
        pyrepl::gui();
        
        console.draw("Repl",&showConsole);
		return false;
	}
    
	void update()
	{
		// Gets called every frame before render
	}
	
	void render()
	{
        float w = appWidth();
        float h = appHeight();
        gfx::clear(0,0,0,0);
        gfx::setOrtho(w,h);
        gfx::setBlendMode(gfx::BLENDMODE_ALPHA);
        gfx::color(1);
        pyrepl::frame();
	}
};

// allows us to access this instance externally if necessary.
#ifndef MAIN_MODULE_SINGLETON
#define MAIN_MODULE_SINGLETON
App * App::instance = 0;
#endif


	
