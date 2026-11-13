#pragma once

#include <world/ChunkData.h>
#include <world/ChunkHolder.h>
#include <world/WorldGen.h>
#include <world/ChunkJobInbox.h>
#include <world/ChunkSnapshot.h>
#include <world/FeatureSnapshot.h>
#include <wv/core.h>
#include <wv/pch.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace ScuffedMinecraft
{
	class ChunkManager
	{
	public:
		ChunkManager(WillowVox::Shader* shader, WillowVox::Camera* camera, int threads, std::shared_ptr<WorldGen> worldGen);
		
		// Requires chunk ID (Main thread only)
		ChunkHolder* GetChunkHolder(const glm::ivec3& id);
		ChunkData* GetChunkData(const glm::ivec3& id);

		// Requires block pos (Main thread only)
		BlockId GetBlockId(const glm::ivec3& blockPos);
		BlockId GetBlockId(int bx, int by, int bz);

		int GetLightLevel(const glm::ivec3& blockPos);
		int GetLightLevel(int bx, int by, int bz);
		int GetSkyLightLevel(const glm::ivec3& blockPos);
		int GetSkyLightLevel(int bx, int by, int bz);

		// Edits (Main thread only)
		void SetBlock(int bx, int by, int bz, BlockId blockId);

		void SetRenderDistance(int renderDistance, int renderHeight);
		void SetSmoothLighting(bool smoothLighting);
		
		// Frame stuff
		void Update();
		void Render();

		// Helper getters (Main thread only)
		int GetChunkHolderCount() const;
		int GetChunkDataCount() const;
		int GetChunkMeshCount() const;

	private:
		bool IsInRenderDistance(const glm::ivec3& pos, const glm::ivec3& cam) const;
		void UpdateChunkEnqueueCache();
		void UpdateChunksWanted(const glm::ivec3& camChunk);

		void MarkChunkDirty(ChunkHolder& holder);

		void LightChunk(ChunkHolder* holder);

		std::vector<ChunkHolder*> GetNeighboringChunks(const glm::ivec3& id);
		std::shared_ptr<ChunkSnapshot> BuildSnapshot(ChunkHolder* holder, const std::vector<ChunkHolder*> neighbors);
		std::shared_ptr<FeatureSnapshot> BuildFeatureSnapshot(ChunkHolder* holder);
		
		void SubmitGenerateChunkJob(int priority, ChunkHolder* holder);
		void SubmitGenerateWorldFeaturesJob(int priority, ChunkHolder* holder);
		void SubmitGenerateMeshJob(int priority, ChunkHolder* holder, const std::vector<ChunkHolder*> neighbors);

		std::shared_ptr<WorldGen> _worldGen;

		int _numThreads;
		ChunkJobInbox _jobResultInbox;
		WillowVox::PriorityJobPool _jobPool;

		std::unordered_map<glm::ivec3, std::unique_ptr<ChunkHolder>> _chunks;
		std::vector<glm::ivec3> _chunksWanted;

		int _chunkKeepDistance = 10;
		int _chunkKeepHeight = 4;
		std::vector<glm::ivec3> _chunkEnqueueCache;
		bool _smoothLighting = true;

		WillowVox::Shader* _shader;
		WillowVox::Renderer* _renderer;

		WillowVox::Camera* _camera;
		glm::ivec3 _lastCamChunk{ 10000 };
		glm::ivec3 _currentCamChunk;

		int _numChunkData = 0;
		int _numChunkMeshes = 0;

		uint64_t _currentToken;
		int _genJobsInFlight = 0;
		int _meshJobsInFlight = 0;
		int _maxGenJobsInFlight;
		int _maxMeshJobsInFlight;
	};
}