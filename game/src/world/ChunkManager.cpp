#include <world/ChunkManager.h>
#include <world/BlockRegistry.h>
#include <world/VoxelLighting.h>
#include <world/WorldCoords.h>
#include <world/BuildChunkMesh.h>
#include <algorithm>

using namespace WillowVox;

namespace ScuffedMinecraft
{
	ChunkManager::ChunkManager(Shader* shader, Camera* camera, int threads, std::shared_ptr<WorldGen> worldGen)
		: _shader(shader), _camera(camera), _numThreads(threads), _worldGen(worldGen), _jobPool(threads)
	{
		_renderer = Renderer::GetInstance();

		_maxGenJobsInFlight = 2 * threads;
		_maxMeshJobsInFlight = 2 * threads;

		UpdateChunkEnqueueCache();
	}

	ChunkHolder* ChunkManager::GetChunkHolder(const glm::ivec3& id)
	{
		auto it = _chunks.find(id);
		if (it != _chunks.end())
			return it->second.get();

		return nullptr;
	}

	ChunkData* ChunkManager::GetChunkData(const glm::ivec3& id)
	{
		auto it = _chunks.find(id);
		if (it != _chunks.end())
			return it->second->data.get();

		return nullptr;
	}

	BlockId ChunkManager::GetBlockId(const glm::ivec3& blockPos)
	{
		auto data = GetChunkData(BlockToChunkId(blockPos));
		if (!data) return INVALID_BLOCK_POS;
		return data->GetBlock(BlockToLocal(blockPos.x), BlockToLocal(blockPos.y), BlockToLocal(blockPos.z));
	}

	BlockId ChunkManager::GetBlockId(int bx, int by, int bz)
	{
		auto data = GetChunkData(BlockToChunkId(bx, by, bz));
		if (!data) return INVALID_BLOCK_POS;
		return data->GetBlock(BlockToLocal(bx), BlockToLocal(by), BlockToLocal(bz));
	}

	int ChunkManager::GetLightLevel(const glm::ivec3& blockPos)
	{
		auto data = GetChunkData(BlockToChunkId(blockPos.x, blockPos.y, blockPos.z));
		if (!data) return 0;
		return data->GetLightLevel(BlockToLocal(blockPos.x), BlockToLocal(blockPos.y), BlockToLocal(blockPos.z));
	}

	int ChunkManager::GetLightLevel(int bx, int by, int bz)
	{
		auto data = GetChunkData(BlockToChunkId(bx, by, bz));
		if (!data) return 0;
		return data->GetLightLevel(BlockToLocal(bx), BlockToLocal(by), BlockToLocal(bz));
	}

	int ChunkManager::GetSkyLightLevel(const glm::ivec3& blockPos)
	{
		auto data = GetChunkData(BlockToChunkId(blockPos.x, blockPos.y, blockPos.z));
		if (!data) return 0;
		return data->GetSkyLightLevel(BlockToLocal(blockPos.x), BlockToLocal(blockPos.y), BlockToLocal(blockPos.z));
	}

	int ChunkManager::GetSkyLightLevel(int bx, int by, int bz)
	{
		auto data = GetChunkData(BlockToChunkId(bx, by, bz));
		if (!data) return 0;
		return data->GetSkyLightLevel(BlockToLocal(bx), BlockToLocal(by), BlockToLocal(bz));
	}

	void ChunkManager::SetBlock(int bx, int by, int bz, BlockId blockId)
	{
		auto chunkHolder = GetChunkHolder(BlockToChunkId(bx, by, bz));
		if (!chunkHolder || chunkHolder->stage < ChunkStage::Lit) return;

		auto chunkData = chunkHolder->data.get();
		auto localBlockPos = BlockToLocal(bx, by, bz);

		BlockId oldBlock = chunkData->SetBlock(localBlockPos.x, localBlockPos.y, localBlockPos.z, blockId);
		if (oldBlock == blockId) return;

		std::vector<ChunkHolder*> chunksModified;
		if (blockId == AIR)
		{
			auto block = BlockRegistry::GetInstance()->GetBlock(oldBlock);
			if (block->emitsLight)
				chunksModified = VoxelLighting::RemoveLightSource(this, chunkHolder, localBlockPos.x, localBlockPos.y, localBlockPos.z);
			else
				chunksModified = VoxelLighting::RemoveLightBlocker(this, chunkHolder, localBlockPos.x, localBlockPos.y, localBlockPos.z);

			auto skyChunksModified = VoxelLighting::RemoveSkyLightBlocker(this, chunkHolder, localBlockPos.x, localBlockPos.y, localBlockPos.z);
			for (auto& id : skyChunksModified)
				chunksModified.push_back(id);
		}
		else
		{
			auto block = BlockRegistry::GetInstance()->GetBlock(blockId);
			if (block->emitsLight)
				chunksModified = VoxelLighting::AddLightSource(this, chunkHolder, localBlockPos.x, localBlockPos.y, localBlockPos.z, block->lightLevel);
			else
				chunksModified = VoxelLighting::AddLightBlocker(this, chunkHolder, localBlockPos.x, localBlockPos.y, localBlockPos.z);

			auto skyChunksModified = VoxelLighting::AddSkyLightBlocker(this, chunkHolder, localBlockPos.x, localBlockPos.y, localBlockPos.z);
			for (auto& id : skyChunksModified)
				chunksModified.push_back(id);
		}

		for (auto& h : chunksModified)
			if (h) MarkChunkDirty(*h);

		if (!chunkHolder->mesh) _numChunkMeshes++;
		chunkHolder->mesh = std::move(BuildChunkMesh(*BuildSnapshot(chunkHolder, GetNeighboringChunks(chunkHolder->id)), _smoothLighting));
		chunkHolder->meshDirty = false;
	}

	void ChunkManager::SetRenderDistance(int renderDistance, int renderHeight)
	{
		_chunkKeepDistance = renderDistance + 2;
		_chunkKeepHeight = renderHeight + 2;
		_lastCamChunk += glm::ivec3(1000);
		UpdateChunkEnqueueCache();
	}

	void ChunkManager::SetSmoothLighting(bool smoothLighting)
	{
		_smoothLighting = smoothLighting;
		for (auto& [id, holder] : _chunks)
			holder->meshDirty = true;
	}

	bool ChunkManager::IsInRenderDistance(const glm::ivec3& pos, const glm::ivec3& cam) const
	{
		glm::ivec3 dist = glm::abs(pos - cam);
		return dist.x <= _chunkKeepDistance && dist.z <= _chunkKeepDistance && dist.y <= _chunkKeepHeight;
	}

	void ChunkManager::MarkChunkDirty(ChunkHolder& holder)
	{
		holder.meshDirty = true;
		holder.dataVersion++;
	}

	void ChunkManager::LightChunk(ChunkHolder* holder)
	{
		auto chunksModified = VoxelLighting::CalculateFullLighting(this, holder);
		for (auto h : chunksModified)
		{
			if (h)
				MarkChunkDirty(*h);
		}
	}

	void ChunkManager::UpdateChunkEnqueueCache()
	{
		_chunkEnqueueCache.clear();
		// Adjust by 2 because there's always a 2 chunk border outside of chunk renderers
		for (int x = -_chunkKeepDistance; x <= _chunkKeepDistance; x++)
		{
			for (int z = -_chunkKeepDistance; z <= _chunkKeepDistance; z++)
			{
				for (int y = -_chunkKeepHeight; y <= _chunkKeepHeight; y++)
				{
					_chunkEnqueueCache.push_back(glm::ivec3(x, y, z));
				}
			}
		}

		// Sort by distance
		std::sort(_chunkEnqueueCache.begin(), _chunkEnqueueCache.end(), [&](const glm::ivec3& a, const glm::ivec3& b) {
			glm::vec3 da = a - glm::ivec3(0), db = b - glm::ivec3(0);
			return glm::dot(da, da) < glm::dot(db, db);
		});
	}

	void ChunkManager::UpdateChunksWanted(const glm::ivec3& camChunk)
	{
		// Recreate chunks wanted
		_chunksWanted.clear();
		for (auto& c : _chunkEnqueueCache)
		{
			_chunksWanted.push_back(c + camChunk);
		}

		// Create new holders
		for (auto& pos : _chunksWanted)
		{
			auto [it, inserted] = _chunks.try_emplace(pos);
			if (inserted)
				it->second = std::make_unique<ChunkHolder>(pos, ++_currentToken);
		}

		// Delete old holders that aren't wanted anymore
		for (auto it = _chunks.begin(); it != _chunks.end();)
		{
			if (!IsInRenderDistance(it->first, camChunk))
			{
				if (it->second->data) _numChunkData--;
				if (it->second->mesh) _numChunkMeshes--;
				it = _chunks.erase(it);
			}
			else ++it;
		}
	}

	std::vector<ChunkHolder*> ChunkManager::GetNeighboringChunks(const glm::ivec3& id)
	{
		std::vector<ChunkHolder*> neighbors(27);
		int i = 0;
		for (int z = id.z - 1; z <= id.z + 1; z++)
			for (int x = id.x - 1; x <= id.x + 1; x++)
				for (int y = id.y - 1; y <= id.y + 1; y++, i++)
					neighbors[i] = GetChunkHolder({ x, y, z });

		return neighbors;
	}

	std::shared_ptr<ChunkSnapshot> ChunkManager::BuildSnapshot(ChunkHolder* holder, const std::vector<ChunkHolder*> neighbors)
	{
		if (!holder->data) return nullptr;

		ChunkSnapshot* snapshot = new ChunkSnapshot();
		snapshot->id = holder->id;
		snapshot->dataVersion = holder->dataVersion;

		// Copy main chunk data
		for (int z = 0; z < CHUNK_SIZE; z++)
		{
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				int src = ChunkData::Index(x, 0, z);
				int dst = PadIndex(x, 0, z);
				memcpy(&snapshot->blocks[dst], holder->data->GetData() + src, CHUNK_SIZE * sizeof(BlockId));
				memcpy(&snapshot->light[dst], holder->data->GetLightData() + src, CHUNK_SIZE);
			}
		}

		// Copy neighboring chunk data
		for (int z = -1; z <= CHUNK_SIZE; z++)
		{
			for (int x = -1; x <= CHUNK_SIZE; x++)
			{
				for (int y = -1; y <= CHUNK_SIZE; y++)
				{
					glm::ivec3 pos(x, y, z);
					auto c = BlockToChunkId(pos);
					if (c == glm::ivec3(0)) { y = CHUNK_SIZE - 1; continue; } // Copied already
					//LOG_INFO("%d. Chunk Stage: %d", (c.y + 1) + 3 * ((c.x + 1) + 3 * (c.z + 1)), neighbors[(c.y + 1) + 3 * ((c.x + 1) + 3 * (c.z + 1))]->stage);
					ChunkData* data = neighbors[(c.y + 1) + 3 * ((c.x + 1) + 3 * (c.z + 1))]->data.get();
					glm::ivec3 nPos = pos & CHUNK_MASK;
					//LOG_INFO("%d %d %d, data?: %d", nPos.x, nPos.y, nPos.z, data != nullptr);
					snapshot->blocks[PadIndex(x, y, z)] = data->GetBlock(nPos.x, nPos.y, nPos.z);
					snapshot->light[PadIndex(x, y, z)] = data->GetRawLightLevel(nPos.x, nPos.y, nPos.z);
				}
			}
		}

		return std::shared_ptr<ChunkSnapshot>(snapshot);
	}

	std::shared_ptr<FeatureSnapshot> ChunkManager::BuildFeatureSnapshot(ChunkHolder* holder)
	{
		if (!holder->data) return nullptr;

		FeatureSnapshot* snapshot = new FeatureSnapshot();
		snapshot->id = holder->id;
		snapshot->dataVersion = holder->dataVersion;
		memcpy(&snapshot->blocks, holder->data->GetData(), CHUNK_VOLUME * sizeof(BlockId));

		// Copy neighboring surface blocks
		int index = 0;
		for (int z = -1; z <= 1; z++)
		{
			for (int x = -1; x <= 1; x++)
			{
				for (int y = -1; y <= 1; y++)
				{
					glm::ivec3 id = holder->id + glm::ivec3{ x, y, z };
					auto n = GetChunkData(id);
					if (!n) continue;
					snapshot->ids[index] = id;

					// Copy surface blocks
					snapshot->surfaceBlocks[index] = n->GetSurfaceBlocks();
					index++;
				}
			}
		}

		return std::shared_ptr<FeatureSnapshot>(snapshot);
	}

	void ChunkManager::SubmitGenerateChunkJob(int priority, ChunkHolder* holder)
	{
		holder->jobInFlight = true;
		holder->stage = ChunkStage::Loading;
		_genJobsInFlight++;
		_jobPool.Submit(priority, [id = holder->id, token = holder->token, worldGen = _worldGen, inbox = &_jobResultInbox] {
			ChunkData* data = new ChunkData(id);
			worldGen->GenerateChunkShape(id, data);
			inbox->Push(std::move(TerrainShapeResult(id, token, std::unique_ptr<ChunkData>(data))));
		});
	}

	void ChunkManager::SubmitGenerateWorldFeaturesJob(int priority, ChunkHolder* holder)
	{
		holder->jobInFlight = true;
		holder->stage = ChunkStage::Loading;
		_genJobsInFlight++;
		_jobPool.Submit(priority, [id = holder->id, token = holder->token, worldGen = _worldGen, inbox = &_jobResultInbox, snapshot = BuildFeatureSnapshot(holder)] {
			worldGen->GenerateChunkFeatures(id, snapshot.get());
			inbox->Push(std::move(TerrainFeaturesResult(id, token, snapshot->dataVersion, std::vector<BlockId>(std::begin(snapshot->blocks), std::end(snapshot->blocks)))));
			});
	}

	void ChunkManager::SubmitGenerateMeshJob(int priority, ChunkHolder* holder, const std::vector<ChunkHolder*> neighbors)
	{
		holder->jobInFlight = true;
		_meshJobsInFlight++;
		_jobPool.Submit(priority, [token = holder->token, snapshot = BuildSnapshot(holder, neighbors), inbox = &_jobResultInbox, smoothLighting = _smoothLighting] {
			
			std::unique_ptr<Mesh> mesh = BuildChunkMesh(*snapshot, smoothLighting);
			inbox->Push(std::move(MeshResult(snapshot->id, token, snapshot->dataVersion, std::move(mesh))));
		});
	}

	void ChunkManager::Update()
	{
		// Rebuild chunks wanted if camera moved chunks
		_currentCamChunk = BlockToChunkId(WorldToBlock(_camera->position));
		if (_currentCamChunk != _lastCamChunk)
		{
			// Camera changed chunk
			_lastCamChunk = _currentCamChunk;
			UpdateChunksWanted(_currentCamChunk);
		}

		// Process job results
		auto results = _jobResultInbox.TakeAll();
		for (auto& result : results)
		{
			if (auto* chunkDataResult = std::get_if<TerrainShapeResult>(&result))
			{
				_genJobsInFlight--;
				auto* holder = GetChunkHolder(chunkDataResult->id);

				if (!holder || holder->stage != ChunkStage::Loading || holder->token != chunkDataResult->token) continue;

				holder->data = std::move(chunkDataResult->data);
				holder->stage = ChunkStage::TerrainShape;
				holder->jobInFlight = false;
				_numChunkData++;
			}
			else if (auto* terrainFeaturesResult = std::get_if<TerrainFeaturesResult>(&result))
			{
				_genJobsInFlight--;
				auto* holder = GetChunkHolder(terrainFeaturesResult->id);

				if (!holder || holder->stage != ChunkStage::Loading || holder->token != terrainFeaturesResult->token) continue;
				if (holder->dataVersion != terrainFeaturesResult->dataVersion)
				{
					holder->stage = ChunkStage::TerrainShape;
					holder->jobInFlight = false;
				}

				holder->data->SetData(terrainFeaturesResult->blocks.data());
				holder->stage = ChunkStage::TerrainFeatures;
				holder->jobInFlight = false;
			}
			else if (auto* meshResult = std::get_if<MeshResult>(&result))
			{
				_meshJobsInFlight--;
				auto* holder = GetChunkHolder(meshResult->id);

				if (!holder || holder->token != meshResult->token) continue;

				if (!holder->mesh) _numChunkMeshes++;
				holder->mesh = std::move(meshResult->mesh);
				holder->jobInFlight = false;
				if (holder->dataVersion == meshResult->dataVersion)
					holder->meshDirty = false;
			}
		}

		// Work on building chunks wanted
		auto lightingDeadline = std::chrono::steady_clock::now() + std::chrono::microseconds(6000);
		bool lightingDone = false;
		for (auto& pos : _chunksWanted)
		{
			if (lightingDone && _genJobsInFlight >= _maxGenJobsInFlight && _meshJobsInFlight >= _maxMeshJobsInFlight) break;

			auto& holder = _chunks[pos];
			if (holder->jobInFlight) continue;

			if (_genJobsInFlight < _maxGenJobsInFlight && holder->stage == ChunkStage::Missing)
			{
				// Generate missing chunk data
				SubmitGenerateChunkJob(1, holder.get());
			}
			else if (_genJobsInFlight < _maxGenJobsInFlight && holder->stage == ChunkStage::TerrainShape)
			{
				// Generate world features if surrounding chunks are generated to shape level
				auto neighbors = GetNeighboringChunks(pos);
				bool readyToGen = true;
				for (auto* n : neighbors)
				{
					if (!n || n->stage < ChunkStage::TerrainShape)
					{
						readyToGen = false;
						break;
					}
				}
				if (!readyToGen) continue;

				SubmitGenerateWorldFeaturesJob(1, holder.get());
			}
			else if (!lightingDone && holder->stage == ChunkStage::TerrainFeatures)
			{
				// Light unlit chunk if chunks around it are at least generated
				auto nxIt = _chunks.find(pos + glm::ivec3(-1, 0, 0));
				auto pxIt = _chunks.find(pos + glm::ivec3(1, 0, 0));
				auto nyIt = _chunks.find(pos + glm::ivec3(0, -1, 0));
				auto pyIt = _chunks.find(pos + glm::ivec3(0, 1, 0));
				auto nzIt = _chunks.find(pos + glm::ivec3(0, 0, -1));
				auto pzIt = _chunks.find(pos + glm::ivec3(0, 0, 1));
				if (nxIt != _chunks.end() && nxIt->second->stage >= ChunkStage::TerrainFeatures &&
					pxIt != _chunks.end() && pxIt->second->stage >= ChunkStage::TerrainFeatures &&
					nyIt != _chunks.end() && nyIt->second->stage >= ChunkStage::TerrainFeatures &&
					pyIt != _chunks.end() && pyIt->second->stage >= ChunkStage::TerrainFeatures &&
					nzIt != _chunks.end() && nzIt->second->stage >= ChunkStage::TerrainFeatures &&
					pzIt != _chunks.end() && pzIt->second->stage >= ChunkStage::TerrainFeatures)
				{
					LightChunk(holder.get());
					holder->stage = ChunkStage::Lit;
					MarkChunkDirty(*holder);
					MarkChunkDirty(*nxIt->second);
					MarkChunkDirty(*pxIt->second);
					MarkChunkDirty(*nyIt->second);
					MarkChunkDirty(*pyIt->second);
					MarkChunkDirty(*nzIt->second);
					MarkChunkDirty(*pzIt->second);
					if (std::chrono::steady_clock::now() >= lightingDeadline)
						lightingDone = true;
				}
			}
			else if (_meshJobsInFlight < _maxMeshJobsInFlight && holder->stage == ChunkStage::Lit && holder->meshDirty)
			{
				// Generate new mesh if chunks around it are lit
				auto neighbors = GetNeighboringChunks(pos);
				bool readyToMesh = true;
				for (auto* n : neighbors)
				{
					if (!n || n->stage < ChunkStage::Lit)
					{
						readyToMesh = false;
						break;
					}
				}
				if (!readyToMesh) continue;

				SubmitGenerateMeshJob(0, holder.get(), neighbors);
			}
		}
	}

	void ChunkManager::Render()
	{
		for (auto& [id, holder] : _chunks)
		{
			if (holder->mesh)
			{
				auto model = glm::translate(glm::mat4(1.0f), (glm::vec3)ChunkToBlock(id));
				_renderer->UpdatePushConstants(*_shader, model);
				_renderer->RenderMesh(*_shader, *holder->mesh);
			}
		}
	}

	int ChunkManager::GetChunkHolderCount() const
	{
		return _chunks.size();
	}

	int ChunkManager::GetChunkDataCount() const
	{
		return _numChunkData;
	}

	int ChunkManager::GetChunkMeshCount() const
	{
		return _numChunkMeshes;
	}
}