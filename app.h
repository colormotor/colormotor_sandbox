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
    float paramWidth=300;
    
    Trigger<bool> saveEps_;
    
	App()
	:
	AppModule("Main Module"),
    params("Settings")
	{
		instance = this;
        params.addBool("Console",&showConsole);
        params.addFloat("paramWidth",&paramWidth,200,600)->noGui();
        params.addEvent("Save EPS..",saveEps_);
        
        params.loadXml(getExecutablePath()+"/settings.xml");
        std::string dt = binarize("basic_icons.ttf","icon_font");
        printf("%s",dt.c_str());
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
        pyrepl::resize(paramWidth, appHeight()-console.inputHeight);
        pyrepl::init();
        
        ui::init();
        return true;
	}

	void exit()
	{
        pyrepl::exit();
        params.saveXml(getExecutablePath()+"/settings.xml");
	}

	bool gui()
	{
        // Resizable settings UI on the side
        static bool show=true;
        ImVec2 size = ImVec2(paramWidth,appHeight());
        ImGui::SetNextWindowSize(size);
        ImGui::SetNextWindowPos(ImVec2(appWidth()-paramWidth,0));
        ImGui::Begin("COLORMOTOR",&show, size, -1.0f, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        ImGui::Button("<",ImVec2(5,appHeight()-10));
        if(ImGui::IsItemActive())
        {
            float dx = ImGui::GetIO().MouseDelta.x;
            paramWidth -= dx;
            if(paramWidth > 400)
                paramWidth=400;
            if(paramWidth < 200)
                paramWidth = 200;
        }
        ImGui::SameLine();
        ImGui::BeginChild("content");
        
        imgui(params); // Creates a UI for the parameters
        bool vis = ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_AllowOverlapMode); //, NULL, true, true);
        if(vis)
        {
            ImGui::Text("Framerate: %g", pyapp::fps());
            ImGui::Text("Script: %s", pyrepl::getScriptPath().c_str());
        }
        
        pyrepl::gui();
        ImGui::EndChild();
        ImGui::End();
        
        // Console
        if(pyrepl::hasErrors())
            console.consoleOpen = true;
        
        console.draw(appWidth()-paramWidth, appHeight()/3);
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
        pyrepl::resize(appWidth()-paramWidth, appHeight()-console.inputHeight);
        
        bool savingEps=false;
        if(saveEps_.isTriggered())
        {
            std::string path;
            if(saveFileDialog(path, "eps"))
            {
                gfx::beginEps(path, Box(0, 0, appWidth(), appHeight()));
                savingEps = true;
            }
        }
        pyrepl::frame();
        if(savingEps)
            gfx::endEps();
	}
};

// allows us to access this instance externally if necessary.
#ifndef MAIN_MODULE_SINGLETON
#define MAIN_MODULE_SINGLETON
App * App::instance = 0;
#endif


	
