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
    
    bool capturing=false;
    std::string capturePath;
    int captureFrame=0;
    Image captureImg;
    
    struct
    {
        V4 col_text;
        V4 col_main;
        V4 col_back;
        V4 col_area;
    } uiColors;

	App()
	:
	AppModule("Main Module"),
    params("Settings")
	{
        int hue = 140;
        float col_main_sat = 180.f/255.f;
        float col_main_val = 161.f/255.f;
        float col_area_sat = 124.f/255.f;
        float col_area_val = 100.f/255.f;
        float col_back_sat = 59.f/255.f;
        float col_back_val = 40.f/255.f;

        uiColors.col_text = (ImVec4)ImColor::HSV(hue/255.f,  20.f/255.f, 235.f/255.f);
        uiColors.col_main = (ImVec4)ImColor::HSV(hue/255.f, col_main_sat, col_main_val);
        uiColors.col_back = (ImVec4)ImColor::HSV(hue/255.f, col_back_sat, col_back_val);
        uiColors.col_area = (ImVec4)ImColor::HSV(hue/255.f, col_area_sat, col_area_val);

		instance = this;
        params.addBool("Console",&showConsole);
        params.addFloat("paramWidth",&paramWidth,200,600)->noGui();
        params.addEvent("Save EPS..",saveEps_);
        params.newChild("UI Colors");
        params.addColor("text", &uiColors.col_text);
        params.addColor("main", &uiColors.col_main);
        params.addColor("back", &uiColors.col_back);
        params.addColor("area", &uiColors.col_area);

        params.loadXml(getExecutablePath()+"/settings.xml");
        std::string dt = binarize("basic_icons.ttf","icon_font");
        printf("%s",dt.c_str());
	}
    
    void replError( const std::string & buf )
    {
        log_mutex.lock();
        console.log(buf.c_str());
        log_mutex.unlock();
    }
    
    void replLog( const std::string & buf )
    {
        //log_mutex.lock();
        console.log(buf.c_str());
        //log_mutex.unlock();
    }
    
    void replReload()
    {
        console.clear();
    }
    
    void startCapture()
    {
        std::string path;
        if(!openFolderDialog(path, "Select Capture Folder..."))
        {
            return;
        }
        
        capturePath = path;
        capturing = true;
        captureFrame = 0;
        captureImg = Image(appWidth()-paramWidth, appHeight()-console.inputHeight, Image::BGRA);
    }
    
    void stopCapture()
    {
        capturing = false;
    }

    void capture()
    {
        if(!capturing)
            return;
        
        captureImg.grabFrameBuffer();
        std::stringstream ss;
        ss << captureFrame << ".png";
        std::string path = joinPath(capturePath, ss.str());
        captureImg.save(path);
        captureFrame++;
        //grabFrameBuffer
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
        params.saveXml(getExecutablePath()+"/settings.xml");
        pyrepl::exit();
	}

	bool gui()
	{
        //static bool opendemo=true;
        //ImGui::ShowDemoWindow(&opendemo);
        //return false;
        ImGui::SetupStyle(  uiColors.col_text,
                            uiColors.col_main,
                            uiColors.col_back,
                            uiColors.col_area);
        // Resizable settings UI on the side
        static bool show=true;
        ImVec2 size = ImVec2(paramWidth, appHeight());
        ImGui::SetNextWindowSize(size);
        ImGui::SetNextWindowPos(ImVec2(appWidth()-paramWidth,0));
        ImGui::Begin("COLORMOTOR", &show, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
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
        
        
        bool vis;
        
        // save frames
        vis = ImGui::CollapsingHeader("Save Frames", ImGuiTreeNodeFlags_AllowItemOverlap); //, NULL, true, true);
        if(vis)
        {
            if(capturing)
            {
                static bool on=true;
                ImGui::Checkbox(" ",&on);
                on = true;
                
                if(ImGui::Button("Stop Capture"))
                {
                    stopCapture();
                }
            }
            else
            {
                if(ImGui::Button("Start Capture..."))
                {
                    startCapture();
                }
            }
        }
        
        vis = ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_AllowItemOverlap); //, NULL, true, true);
        if(vis)
        {
            ImGui::Text("Framerate: %g", pyapp::fps());
            ImGui::Text("Script: %s", pyrepl::getScriptPath().c_str());
        }
        
#ifdef OSC_ENABLED

#endif
        
        
        pyrepl::gui();
        ImGui::EndChild();
        ImGui::End();
        
        log_mutex.lock();
        // Console
        if(pyrepl::hasErrors())
            console.consoleOpen = true;
        
        console.draw(appWidth()-paramWidth, appHeight()/3);
        log_mutex.unlock();
        
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
        
        capture();
	}
};

// allows us to access this instance externally if necessary.
#ifndef MAIN_MODULE_SINGLETON
#define MAIN_MODULE_SINGLETON
App * App::instance = 0;
#endif


	
