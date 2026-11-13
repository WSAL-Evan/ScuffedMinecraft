#include <wv/core.h>

#include <physics/VoxelRaycast.h>
#include <world/ChunkData.h>
#include <world/BlockDisplay.h>
#include <world/ChunkManager.h>
#include <world/Block.h>
#include <world/BlockRegistry.h>
#include <world/ChunkVertex.h>
#include <world/WorldCoords.h>
#include <FastNoise/FastNoise.h>
#include <cpu_count.h>
#include <algorithm>
#include <cstdlib>
#include <imgui.h>

using namespace WillowVox;

namespace ScuffedMinecraft
{
    class ScuffedMinecraftApp : public WillowVox::App 
    {
    protected:
        void Start() override
        {
            LOG_INFO("ScuffedMinecraftApp started!");

            _renderer->SetClearColor({ 0.4f, 0.6f, 0.9f, 1.0f });

            // Load assets
            _shader = std::unique_ptr<Shader>(_renderer->CreateShader<ChunkVertex>("assets/shaders/shader.spv"));

            // Register blocks
            _blockRegistry = BlockRegistry::GetInstance();
            BlockId dirtBlockId = _blockRegistry->RegisterBlock({ "Dirt Block" }, { 0.0f, 0.25f, 0.0f, 0.25f });
            BlockId grassBlockId = _blockRegistry->RegisterBlock({ "Grass Block" },
                { 0.25f, 0.5f, 0.0f, 0.25f,  0.0f, 0.25f, 0.0f, 0.25f,  0.0f, 0.25f, 0.25f, 0.5f });
            BlockId stoneBlockId = _blockRegistry->RegisterBlock({ "Stone Block" }, { 0.25f, 0.5f, 0.25f, 0.5f });
            BlockId glowstoneId = _blockRegistry->RegisterBlock({ "Glowstone", false, 0, true, 15 }, { 0.5f, 0.75f, 0.0f, 0.25f });
            BlockId logId = _blockRegistry->RegisterBlock({ "Log" }, { 0.75f, 1.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.0f, 0.25f });
            BlockId leavesId = _blockRegistry->RegisterBlock({ "Leaves", true, 4 }, { 0.5f, 0.75f, 0.25f, 0.5f });
            BlockId grassId = _blockRegistry->RegisterBlock({ "Grass", true }, { 0.0f, 0.25f, 0.5f, 0.75f, true });
            BlockId dandelionId = _blockRegistry->RegisterBlock({ "Dandelion", true }, { 0.25f, 0.5f, 0.5f, 0.75f, true });
            BlockId poppyId = _blockRegistry->RegisterBlock({ "Poppy", true }, { 0.5f, 0.75f, 0.5f, 0.75f, true });
            BlockId blueOrchidId = _blockRegistry->RegisterBlock({ "Blue Orchid", true }, { 0.75f, 1.0f, 0.5f, 0.75f, true });
            BlockId tallGrassBottomId = _blockRegistry->RegisterBlock({ "Tall Grass Bottom", true }, { 0.0f, 0.25f, 0.75f, 1.0f, true });
            BlockId tallGrassTopId = _blockRegistry->RegisterBlock({ "Tall Grass Top", true }, { 0.25f, 0.5f, 0.75f, 1.0f, true });

            _selectedBlock = glowstoneId;

            _camera = std::make_unique<Camera>();
            _camera->position = { 0.0f, 15.0f, 0.0f };

            Input::SetMouseMode(MouseMode::DISABLED);

            // Create world gen
            _worldGen = std::make_shared<WorldGen>(
                0.0f, 10.0f, 100.0f, 2, 0.3f, 4.0f,
                30.0f, 2, 0.25f, 3.0f, 0.8f,
                std::vector<WorldFeature>{ 
                WorldFeature{
                    .layers = {
                        {
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, dirtBlockId, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                        },
                        {
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, logId, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                        },
                        {
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, logId, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                        },
                        {
                            { AIR, leavesId, leavesId, leavesId, AIR },
                            { leavesId, leavesId, leavesId, leavesId, leavesId },
                            { leavesId, leavesId, logId, leavesId, leavesId },
                            { leavesId, leavesId, leavesId, leavesId, leavesId },
                            { AIR, leavesId, leavesId, leavesId, AIR },
                        },
                        {
                            { AIR, leavesId, leavesId, leavesId, AIR },
                            { leavesId, leavesId, leavesId, leavesId, leavesId },
                            { leavesId, leavesId, logId, leavesId, leavesId },
                            { leavesId, leavesId, leavesId, leavesId, leavesId },
                            { AIR, leavesId, leavesId, leavesId, AIR },
                        },
                        {
                            { AIR, AIR, AIR, AIR, AIR },
                            { AIR, leavesId, leavesId, leavesId, AIR },
                            { AIR, leavesId, logId, leavesId, AIR },
                            { AIR, leavesId, leavesId, leavesId, AIR },
                            { AIR, AIR, AIR, AIR, AIR },
                        },
                        {
                            { AIR, AIR,      AIR,      AIR,      AIR },
                            { AIR, AIR,      leavesId, AIR,      AIR },
                            { AIR, leavesId, leavesId, leavesId, AIR },
                            { AIR, AIR,      leavesId, AIR,      AIR },
                            { AIR, AIR,      AIR,      AIR,      AIR },
                        }
                    },
                    .replace = {
                        {
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, true, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                        },
                        {
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, true, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                        },
                        {
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, true, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                        },
                        {
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, true, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                        },
                        {
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, true, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                        },
                        {
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, true, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                        },
                        {
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                            { false, false, false, false, false },
                        }
                    },
                    .center = { -2, 0, -2 },
                    .placementChance = 0.01f },

                WorldFeature{
                    .layers = { { { grassId } } },
                    .replace = { { { false } } },
                    .center = { 0, 1, 0 },
                    .placementChance = 0.1f
                },
                WorldFeature{
                   .layers = { { { dandelionId } } },
                   .replace = { { { false } } },
                   .center = { 0, 1, 0 },
                   .placementChance = 0.01f
                },
                WorldFeature{
                   .layers = { { { poppyId } } },
                   .replace = { { { false } } },
                   .center = { 0, 1, 0 },
                   .placementChance = 0.01f
                },
                WorldFeature{
                   .layers = { { { blueOrchidId } } },
                   .replace = { { { false } } },
                   .center = { 0, 1, 0 },
                   .placementChance = 0.01f
                },
                WorldFeature{
                   .layers = { { { tallGrassBottomId } },
                                { { tallGrassTopId } } },
                   .replace = { { { false } },
                                { { false } } },
                   .center = { 0, 1, 0 },
                   .placementChance = 0.01f
                }
            });

            // Create chunk manager
            LOG_INFO("Physical CPU Cores: %d", cpu_count());
            _chunkManager = std::make_unique<ChunkManager>(_shader.get(), _camera.get(), std::max(1, (int)cpu_count() - 2), _worldGen);
            _chunkManager->SetRenderDistance(_renderDistance, _renderHeight);

            _start = std::chrono::steady_clock::now();
        }

        void Update(float deltaTime) override
        {
            _frames++;
            _frameTimer += deltaTime;
            if (_frameTimer >= 1.0f)
            {
                _fps = _frames;
                _frames = 0;
                _frameTimer -= 1.0f;
            }

            ProcessInput(deltaTime);

            _chunkManager->Update();

            if (_generating && _chunkManager->GetChunkMeshCount() >= 1445)
            {
                auto end = std::chrono::steady_clock::now();
                std::chrono::duration<double, std::milli> elapsed = end - _start;
                LOG_INFO("Generation took %f ms", elapsed);
                _generating = false;
            }
        }

        void ProcessInput(float deltaTime)
        {
            if (Input::GetKeyDown(Key::ESC))
            {
                _movementDisabled = !_movementDisabled;
                if (_movementDisabled)
                {
                    _firstFrame = true;
                    Input::SetMouseMode(MouseMode::NORMAL);
                }
                else
                    Input::SetMouseMode(MouseMode::DISABLED);
            }

            if (_movementDisabled)
                return;

            if (Input::GetKey(Key::SPACE))
            {
                if (Input::GetKey(Key::W))
                {
                    auto front = _camera->Front();
                    front.y = 0.0f;
                    front = glm::normalize(front);
                    _camera->position += front * _movementSpeed * deltaTime;
                }
                if (Input::GetKey(Key::S))
                {
                    auto front = _camera->Front();
                    front.y = 0.0f;
                    front = glm::normalize(front);
                    _camera->position -= front * _movementSpeed * deltaTime;
                }
            }
            else
            {
                if (Input::GetKey(Key::W))
                    _camera->position += _camera->Front() * _movementSpeed * deltaTime;
                if (Input::GetKey(Key::S))
                    _camera->position -= _camera->Front() * _movementSpeed * deltaTime;
            }
            if (Input::GetKey(Key::A))
                _camera->position -= _camera->Right() * _movementSpeed * deltaTime;
            if (Input::GetKey(Key::D))
                _camera->position += _camera->Right() * _movementSpeed * deltaTime;
            if (Input::GetKey(Key::E))
                _camera->position += _camera->Up() * _movementSpeed * deltaTime;
            if (Input::GetKey(Key::Q))
                _camera->position -= _camera->Up() * _movementSpeed * deltaTime;

            if (_firstFrame)
                _firstFrame = false;
            else
            {
                auto mouseMove = Input::GetMouseDelta();
                _camera->direction.y += mouseMove.x * _mouseSensitivity;
                _camera->direction.x -= mouseMove.y * _mouseSensitivity;
                if (_camera->direction.x >= 89.9f)
                    _camera->direction.x = 89.9f;
                else if (_camera->direction.x <= -89.9f)
                    _camera->direction.x = -89.9f;

                auto scrollDelta = Input::GetMouseScrollDelta();
                _movementSpeed += scrollDelta.y;
                if (_movementSpeed < 0.0f)
                    _movementSpeed = 0.0f;
                else if (_movementSpeed > 100.0f)
                    _movementSpeed = 100.0f;
            }

            if (Input::GetMouseButtonDown(0))
            {
                auto result = VoxelRaycast(*_chunkManager, _camera->position, _camera->Front(), 10.0f);

                if (result.hit)
                {
                    auto blockPos = WorldToBlock(result.hitX, result.hitY, result.hitZ);
                    _chunkManager->SetBlock(blockPos.x, blockPos.y, blockPos.z, AIR);
                }
            }
            else if (Input::GetMouseButtonDown(1))
            {
                auto result = VoxelRaycast(*_chunkManager, _camera->position, _camera->Front(), 10.0f);

                if (result.hit)
                {
                    auto block = WorldToBlock(result.hitX, result.hitY, result.hitZ);
                    float distX = result.hitX - (block.x + .5f);
                    float distY = result.hitY - (block.y + .5f);
                    float distZ = result.hitZ - (block.z + .5f);

                    int blockX = block.x;
                    int blockY = block.y;
                    int blockZ = block.z;

                    // Choose face to place on
                    if (std::fabs(distX) > std::fabs(distY) && std::fabs(distX) > std::fabs(distZ))
                        blockX += (distX > 0 ? 1 : -1);
                    else if (std::fabs(distY) > std::fabs(distX) && std::fabs(distY) > std::fabs(distZ))
                        blockY += (distY > 0 ? 1 : -1);
                    else
                        blockZ += (distZ > 0 ? 1 : -1);

                    _chunkManager->SetBlock(blockX, blockY, blockZ, _selectedBlock);
                }
            }
            else if (Input::GetMouseButtonDown(2))
            {
                auto result = VoxelRaycast(*_chunkManager, _camera->position, _camera->Front(), 10.0f);

                if (result.hit)
                    _selectedBlock = _chunkManager->GetBlockId(WorldToBlock(result.hitX, result.hitY, result.hitZ));
            }
        }

        void Render() override
        {
            _chunkManager->Render();

            _renderer->UpdateUniformBuffer(*_camera);
        }

        void DrawUI() override
        {
            ImGui::Begin("Scuffed Minecraft");

            ImGui::Text("FPS: %d", _fps);
            ImGui::Text("Chunk Holders: %d (Data: %d, Renderers: %d)", _chunkManager->GetChunkHolderCount(), _chunkManager->GetChunkDataCount(), _chunkManager->GetChunkMeshCount());
            ImGui::Text("Cam Pos: (%f, %f, %f)", _camera->position.x, _camera->position.y, _camera->position.z);
            ImGui::Text("Selected Block: %d (%s)", _selectedBlock, _blockRegistry->GetBlock(_selectedBlock)->name.c_str());

            auto camBlockPos = WorldToBlock(_camera->position);
            ImGui::Text("Light Value: %d (Sky: %d)", _chunkManager->GetLightLevel(camBlockPos), _chunkManager->GetSkyLightLevel(camBlockPos));

            if (ImGui::SliderInt("Render Distance", &_renderDistance, 1, 25))
                _chunkManager->SetRenderDistance(_renderDistance, _renderHeight);
            if (ImGui::SliderInt("Render Height", &_renderHeight, 1, 10))
                _chunkManager->SetRenderDistance(_renderDistance, _renderHeight);
            if (ImGui::Checkbox("Smooth Lighting", &_smoothLighting))
                _chunkManager->SetSmoothLighting(_smoothLighting);

            ImGui::End();
        }

    private:
        std::unique_ptr<Shader> _shader;
        std::unique_ptr<Camera> _camera;
        std::shared_ptr<WorldGen> _worldGen;
        std::unique_ptr<ChunkManager> _chunkManager;
        BlockRegistry* _blockRegistry;

        bool _movementDisabled = false;
        float _movementSpeed = 10.0f;
        float _mouseSensitivity = 0.3f;
        bool _firstFrame = true;

        int _frames = 0;
        float _frameTimer = 0;
        int _fps = 0;

        int _renderDistance = 8;
        int _renderHeight = 2;
        bool _smoothLighting = true;

        BlockId _selectedBlock;

        std::chrono::steady_clock::time_point _start;
        bool _generating = true;
    };
}

#include <wv/app/EntryPoint.h>

WillowVox::App* WillowVox::CreateApp()
{
    auto app = new ScuffedMinecraft::ScuffedMinecraftApp();

    app->config.name = "ScuffedMinecraft";
    app->config.width = 1920;
    app->config.height = 1080;

    return app;
}