#pragma once

#include <wv/rendering/Renderer.h>
#include <wv/rendering/Window.h>
#include <wv/pch.h>

namespace WillowVox
{
    class RenderSystem;

    struct AppConfig {
        const char* name = "WillowVox Application";
        uint32_t width = 1280;
        uint32_t height = 720;
    };

    // This is the base application class
    // The project must create a subclass of this
    class App
    {
    public:
        AppConfig config; // This should be set in the CreateApp implementation

        App() = default;
        virtual ~App();

        // To be used by the engine. Starts the application
        void Run();

    protected:
        // Runs at the start of the project
        virtual void Start() {}
        // Runs every frame
        virtual void Update(float deltaTime) {}
        // Runs at the end of every frame for custom rendering code (only to be used for advanced reasons)
        virtual void Render() {}
        // Runs after rendering. Use ImGui
        virtual void DrawUI() {}

        std::unique_ptr<Renderer> _renderer;
        std::unique_ptr<Window> _window;

    private:
        double _lastFrame = 0.0f;
    };

    App* CreateApp();
}