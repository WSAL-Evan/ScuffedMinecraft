#include <wv/app/App.h>

#include <wv/input/Input.h>
#include <wv/Logger.h>
#include <wv/rendering/Renderer.h>

namespace WillowVox
{
    void App::Run()
    {
        LOG_SUCCESS("Using WillowVox Engine v0.1.0");
        LOG_INFO("Starting %s", config.name);

        // Initialize rendering variables
        _window = std::make_unique<Window>(config.name, config.width, config.height);
        Input::Init();

        _renderer = std::make_unique<Renderer>(_window.get());

        // User-defined start logic
        Start();

        LOG_SUCCESS("Application successfully started");

        // Main Loop
        while (!_window->ShouldClose())
        {
            Input::ResetStates();

            _window->PollEvents();

            double currentTime = _window->GetTime();
            double deltaTime = currentTime - _lastFrame;
            _lastFrame = currentTime;

            Update(deltaTime);

            if (_renderer->BeginRendering())
            {
                Render();
                _renderer->BeginUI();
                DrawUI();
                _renderer->EndRendering();
            }
        }

        _renderer->Stop();
    }

    App::~App()
    {
        _renderer.reset();
        _window.reset();

        LOG_INFO("Application successfully shut down");
    }
}