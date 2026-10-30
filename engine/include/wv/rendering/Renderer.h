#pragma once

#include <wv/rendering/Window.h>
#include <wv/rendering/Vertex.h>
#include <wv/rendering/Shader.h>
#include <wv/rendering/Mesh.h>
#include <wv/rendering/VertexAttribute.h>
#include <wv/pch.h>

namespace WillowVox
{
    class Camera;



    class Renderer
    {
    public:
        Renderer(Window* window);
        ~Renderer();

        static Renderer* GetInstance()
        {
            return s_instance;
        }

        bool BeginRendering();
        void BeginUI();
        void EndRendering();

        void RenderMesh(const Shader& shader, const Mesh& mesh);

        Shader* CreateShader(const std::string& shaderPath, uint32_t vertexSize, const std::vector<VertexAttribute>& attributes);
        Mesh* CreateMesh(const void* vertices, uint32_t verticesSize, const uint32_t* indices, uint32_t indexCount);

        // The vertex type must have a static function called GetVertexAttributes, which returns a vector of vertex attributes
        template <typename V>
        Shader* CreateShader(const std::string& shaderPath)
        {
            return CreateShader(shaderPath, sizeof(V), V::GetVertexAttributes());
        }

        void DestroyMesh(Mesh* mesh);

        void UpdateUniformBuffer(const Camera& camera);
        void UpdatePushConstants(const Shader& shader, const glm::mat4& model);

        void SetClearColor(const glm::vec4& color);

        void Stop();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;

        static Renderer* s_instance;
    };
}