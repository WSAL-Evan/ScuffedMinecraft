#include <world/VoxelLighting.h>
#include <world/ChunkManager.h>
#include <world/BlockRegistry.h>
#include <world/WorldCoords.h>
#include <world/Direction.h>
#include <wv/pch.h>

namespace ScuffedMinecraft
{
	namespace VoxelLighting
	{
		struct LightNode
		{
			uint16_t index;
			uint16_t chunk;
			uint8_t dirFlags;

			LightNode(int index, uint16_t chunk, uint8_t dirFlags)
				: index(index), chunk(chunk), dirFlags(dirFlags) {}
		};

		struct LightRemovalNode
		{
			uint16_t index_light;
			uint16_t chunk;
			uint8_t dirFlags;

			int Index() const { return index_light & 0xFFF; }
			int Light() const { return (index_light >> 12) & 0xF; }

			LightRemovalNode(int index, int light, uint16_t chunk, uint8_t dirFlags)
				: index_light(((uint16_t)light << 12) | (uint16_t)index), chunk(chunk), dirFlags(dirFlags) {}
		};

		static inline bool ShouldPropagateInDir(uint8_t dirFlags, Direction dir)
		{
			return (dirFlags & ((uint8_t)1 << dir)) != 0;
		}

		static constexpr uint8_t PropAllDir() { return 0xFF; }

		static constexpr uint8_t PropAllButOneDir(Direction dir) { return (uint8_t)0xFF & (((uint8_t)1 << dir) ^ 0xFF); }

		static constexpr uint8_t PropInOneDir(Direction dir) { return 1 << dir; }

		static inline uint8_t PropInDirs(bool negX, bool posX, bool negY, bool posY, bool negZ, bool posZ)
		{
			return (uint8_t)posZ << 5 | (uint8_t)negZ << 4 | (uint8_t)posY << 3 | (uint8_t)negY << 2 | (uint8_t)posX << 1 | (uint8_t)negX;
		}

		static_assert(Opposite(NEG_X) == POS_X);
		static_assert(Opposite(POS_X) == NEG_X);
		static_assert(PropAllDir() == 0b11111111);
		static_assert(PropAllButOneDir(NEG_X) == 0b11111110);
		static_assert(PropAllButOneDir(POS_Y) == 0b11110111);
		static_assert(PropInOneDir(NEG_X) == 0b00000001);
		static_assert(PropInOneDir(NEG_Z) == 0b00010000);

		struct BlockChannel
		{
			static int  Get(const ChunkData& c, int i) { return c.GetLightLevel(i); }
			static void Set(ChunkData& c, int i, int v) { c.SetLightLevel(i, v); }
			static constexpr bool straightDown = false;
		};
		struct SkyChannel
		{
			static int  Get(const ChunkData& c, int i) { return c.GetSkyLightLevel(i); }
			static void Set(ChunkData& c, int i, int v) { c.SetSkyLightLevel(i, v); }
			static constexpr bool straightDown = true;
		};

		template <class T>
		int Incoming(int from, Direction d)
		{
			return (T::straightDown && d == NEG_Y && from == 15) ? 15 : from - 1;
		}

		static inline uint16_t Step(ChunkManager* chunkManager, std::vector<ChunkHolder*>& chunks, uint16_t chunkIndex, ChunkData* data, glm::ivec3& pos, Direction dir)
		{
			glm::ivec3 p = pos + dirOffsets[dir];
			if (ChunkData::InBounds(p.x, p.y, p.z))
			{
				pos = p;
				return chunkIndex;
			}

			glm::ivec3 cid = data->GetId() + dirOffsets[dir];
			pos = p & CHUNK_MASK;

			for (int i = 0; i < chunks.size(); i++)
			{
				if (chunks[i] && chunks[i]->id == cid) return i;
			}

			chunks.push_back(chunkManager->GetChunkHolder(cid));
			return chunks.size() - 1;
		}

		template <class T>
		void PropagateLight(ChunkManager* chunkManager, std::queue<LightNode>& queue, std::vector<ChunkHolder*>& chunksTouched)
		{
			while (!queue.empty())
			{
				auto node = queue.front();
				queue.pop();
				if (!chunksTouched[node.chunk] || !chunksTouched[node.chunk]->data) continue;
				ChunkData* data = chunksTouched[node.chunk]->data.get();

				int level = T::Get(*data, node.index);
				if (level <= 1) continue;

				glm::ivec3 pos = { ChunkData::GetX(node.index), ChunkData::GetY(node.index), ChunkData::GetZ(node.index) };

				for (int d = 0; d < 6; d++)
				{
					if (!ShouldPropagateInDir(node.dirFlags, Direction(d))) continue;

					glm::ivec3 nPos = pos;
					uint16_t nChunk = Step(chunkManager, chunksTouched, node.chunk, data, nPos, Direction(d));
					if (!chunksTouched[nChunk] || !chunksTouched[nChunk]->data) continue;
					ChunkData* nData = chunksTouched[nChunk]->data.get();

					int nBlockIndex = ChunkData::Index(nPos.x, nPos.y, nPos.z);
					BlockId nBlock = nData->GetBlock(nBlockIndex);

					if (nBlock != AIR) continue;

					int nLevel = T::Get(*nData, nBlockIndex);
					int incoming = Incoming<T>(level, Direction(d));
					if (incoming > nLevel)
					{
						T::Set(*nData, nBlockIndex, incoming);
						queue.emplace(nBlockIndex, nChunk, PropAllButOneDir(Opposite((Direction(d)))));
					}
				}
			}
		}

		template <class T>
		void PropagateDarkness(ChunkManager* chunkManager, std::queue<LightRemovalNode>& queue, std::vector<ChunkHolder*>& chunksTouched)
		{
			std::queue<LightNode> propagationQueue;
			while (!queue.empty())
			{
				auto node = queue.front();
				queue.pop();
				if (!chunksTouched[node.chunk] || !chunksTouched[node.chunk]->data) continue;
				ChunkData* data = chunksTouched[node.chunk]->data.get();

				int index = node.Index();
				int level = node.Light();
				if (level == 0) continue;

				glm::ivec3 pos = { ChunkData::GetX(index), ChunkData::GetY(index), ChunkData::GetZ(index) };

				for (int d = 0; d < 6; d++)
				{
					if (!ShouldPropagateInDir(node.dirFlags, Direction(d))) continue;

					glm::ivec3 nPos = pos;
					uint16_t nChunk = Step(chunkManager, chunksTouched, node.chunk, data, nPos, Direction(d));
					if (!chunksTouched[nChunk] || !chunksTouched[nChunk]->data) continue;
					ChunkData* nData = chunksTouched[nChunk]->data.get();

					int nBlockIndex = ChunkData::Index(nPos.x, nPos.y, nPos.z);
					int nLevel = T::Get(*nData, nBlockIndex);
					if (nLevel == 0) continue;

					BlockId nBlock = nData->GetBlock(nBlockIndex);
					if (nBlock == AIR && (nLevel < level || (T::straightDown && d == NEG_Y && level == 15 && nLevel == 15)))
					{
						T::Set(*nData, nBlockIndex, 0);
						queue.emplace(nBlockIndex, nLevel, nChunk, PropAllButOneDir(Opposite((Direction(d)))));
					}
					else
						propagationQueue.emplace(nBlockIndex, nChunk, PropAllDir());
				}
			}

			PropagateLight<T>(chunkManager, propagationQueue, chunksTouched);
		}

		std::vector<ChunkHolder*> AddLightSource(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z, int lightLevel)
		{
			std::vector<ChunkHolder*> chunksTouched;
			chunksTouched.push_back(chunkHolder);

			int index = ChunkData::Index(x, y, z);
			chunkHolder->data->SetLightLevel(index, lightLevel);

			std::queue<LightNode> propagationQueue;
			propagationQueue.emplace(index, 0, PropAllDir());
			PropagateLight<BlockChannel>(chunkManager, propagationQueue, chunksTouched);
			return chunksTouched;
		}

		std::vector<ChunkHolder*> RemoveLightSource(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z)
		{
			std::vector<ChunkHolder*> chunksTouched;
			chunksTouched.push_back(chunkHolder);

			int index = ChunkData::Index(x, y, z);
			int lightLevel = chunkHolder->data->GetLightLevel(index);
			chunkHolder->data->SetLightLevel(index, 0);

			std::queue<LightRemovalNode> propagationQueue;
			propagationQueue.emplace(index, lightLevel, 0, PropAllDir());
			PropagateDarkness<BlockChannel>(chunkManager, propagationQueue, chunksTouched);
			return chunksTouched;
		}

		std::vector<ChunkHolder*> AddLightBlocker(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z)
		{
			return RemoveLightSource(chunkManager, chunkHolder, x, y, z);
		}

		std::vector<ChunkHolder*> RemoveLightBlocker(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z)
		{
			std::vector<ChunkHolder*> chunksTouched;
			chunksTouched.push_back(chunkHolder);

			std::queue<LightNode> propagationQueue;

			for (int d = 0; d < 6; d++)
			{
				glm::ivec3 p = glm::ivec3(x, y, z);
				uint16_t index = Step(chunkManager, chunksTouched, 0, chunkHolder->data.get(), p, Direction(d));
				propagationQueue.emplace(ChunkData::Index(p.x, p.y, p.z), index, PropInOneDir(Opposite(Direction(d))));
			}

			PropagateLight<BlockChannel>(chunkManager, propagationQueue, chunksTouched);
			return chunksTouched;
		}

		std::vector<ChunkHolder*> AddSkyLightBlocker(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z)
		{
			std::vector<ChunkHolder*> chunksTouched;
			chunksTouched.push_back(chunkHolder);

			int index = ChunkData::Index(x, y, z);
			int lightLevel = chunkHolder->data->GetSkyLightLevel(index);
			chunkHolder->data->SetSkyLightLevel(index, 0);

			std::queue<LightRemovalNode> propagationQueue;
			propagationQueue.emplace(index, lightLevel, 0, PropAllDir());
			PropagateDarkness<SkyChannel>(chunkManager, propagationQueue, chunksTouched);
			return chunksTouched;
		}

		std::vector<ChunkHolder*> RemoveSkyLightBlocker(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z)
		{
			std::vector<ChunkHolder*> chunksTouched;
			chunksTouched.push_back(chunkHolder);

			std::queue<LightNode> propagationQueue;

			for (int d = 0; d < 6; d++)
			{
				glm::ivec3 p = glm::ivec3(x, y, z);
				uint16_t index = Step(chunkManager, chunksTouched, 0, chunkHolder->data.get(), p, Direction(d));
				propagationQueue.emplace(ChunkData::Index(p.x, p.y, p.z), index, PropInOneDir(Opposite(Direction(d))));
			}

			PropagateLight<SkyChannel>(chunkManager, propagationQueue, chunksTouched);
			return chunksTouched;
		}

		std::vector<ChunkHolder*> CalculateFullLighting(ChunkManager* chunkManager, ChunkHolder* chunkHolder)
		{
			std::vector<ChunkHolder*> chunksTouched;
			chunksTouched.push_back(chunkHolder);

			static auto* blockManager = BlockRegistry::GetInstance();

			std::queue<LightNode> propagationQueue;
			std::queue<LightNode> skyPropagationQueue;

			ChunkData* data = chunkHolder->data.get();

			// Count all solid blocks and light emitters
			int solidBlocks = 0;
			int i = 0;
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				for (int x = 0; x < CHUNK_SIZE; x++)
				{
					for (int y = 0; y < CHUNK_SIZE; y++)
					{
						BlockId id = data->GetBlock(i);
						if (id != AIR)
						{
							solidBlocks++;
							Block* block = blockManager->GetBlock(id);
							if (block->emitsLight)
							{
								data->SetLightLevel(i, block->lightLevel);
								propagationQueue.emplace(i, 0, PropAllDir());
							}
						}

						i++;
					}
				}
			}

			// If chunk is higher than sea level and chunk is empty, set all to skylight (temporary)
			if (data->GetId().y >= 0 && solidBlocks == 0)
			{
				int si = 0;
				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					for (int x = 0; x < CHUNK_SIZE; x++)
					{
						for (int y = 0; y < CHUNK_SIZE; y++)
						{
							data->SetSkyLightLevel(si, 15);
							if (x == 0 || x == CHUNK_SIZE - 1
								|| y == 0
								|| z == 0 || z == CHUNK_SIZE - 1)
								skyPropagationQueue.emplace(si, 0, PropInDirs(x == 0, x == CHUNK_SIZE - 1, 
									y == 0, false, z == 0, z == CHUNK_SIZE - 1));

							si++;
						}
					}
				}
			}

			// Get block and sky light from neighboring chunks if they exist
			// Positive X
			auto holder = chunkManager->GetChunkHolder(data->GetId() + glm::ivec3(1, 0, 0));
			if (holder && holder->data)
			{
				int cIndex = chunksTouched.size();
				chunksTouched.push_back(holder);
				auto nData = holder->data.get();

				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					for (int y = 0; y < CHUNK_SIZE; y++)
					{
						int blockLight = data->GetLightLevel(CHUNK_SIZE - 1, y, z);
						int nBlockLight = nData->GetLightLevel(0, y, z);
						int skyLight = data->GetSkyLightLevel(CHUNK_SIZE - 1, y, z);
						int nSkyLight = nData->GetSkyLightLevel(0, y, z);
						if (nBlockLight > blockLight + 1)
							propagationQueue.emplace(ChunkData::Index(0, y, z), cIndex, PropInOneDir(NEG_X));
						if (nSkyLight > skyLight + 1)
							skyPropagationQueue.emplace(ChunkData::Index(0, y, z), cIndex, PropInOneDir(NEG_X));
					}
				}
			}

			// Negative X
			holder = chunkManager->GetChunkHolder(data->GetId() + glm::ivec3(-1, 0, 0));
			if (holder && holder->data)
			{
				int cIndex = chunksTouched.size();
				chunksTouched.push_back(holder);
				auto nData = holder->data.get();

				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					for (int y = 0; y < CHUNK_SIZE; y++)
					{
						int blockLight = data->GetLightLevel(0, y, z);
						int nBlockLight = nData->GetLightLevel(CHUNK_SIZE - 1, y, z);
						int skyLight = data->GetSkyLightLevel(0, y, z);
						int nSkyLight = nData->GetSkyLightLevel(CHUNK_SIZE - 1, y, z);
						if (nBlockLight > blockLight + 1)
							propagationQueue.emplace(ChunkData::Index(CHUNK_SIZE - 1, y, z), cIndex, PropInOneDir(POS_X));
						if (nSkyLight > skyLight + 1)
							skyPropagationQueue.emplace(ChunkData::Index(CHUNK_SIZE - 1, y, z), cIndex, PropInOneDir(POS_X));
					}
				}
			}

			// Positive Y
			holder = chunkManager->GetChunkHolder(data->GetId() + glm::ivec3(0, 1, 0));
			if (holder && holder->data)
			{
				int cIndex = chunksTouched.size();
				chunksTouched.push_back(holder);
				auto nData = holder->data.get();

				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					for (int x = 0; x < CHUNK_SIZE; x++)
					{
						int blockLight = data->GetLightLevel(x, CHUNK_SIZE - 1, z);
						int nBlockLight = nData->GetLightLevel(x, 0, z);
						int skyLight = data->GetSkyLightLevel(x, CHUNK_SIZE - 1, z);
						int nSkyLight = nData->GetSkyLightLevel(x, 0, z);
						if (nBlockLight > blockLight + 1)
							propagationQueue.emplace(ChunkData::Index(x, 0, z), cIndex, PropInOneDir(NEG_Y));
						if (nSkyLight == 15 && skyLight != 15)
							skyPropagationQueue.emplace(ChunkData::Index(x, 0, z), cIndex, PropInOneDir(NEG_Y));
						else if (nSkyLight > skyLight + 1)
							skyPropagationQueue.emplace(ChunkData::Index(x, 0, z), cIndex, PropInOneDir(NEG_Y));
					}
				}
			}

			// Negative Y
			holder = chunkManager->GetChunkHolder(data->GetId() + glm::ivec3(0, -1, 0));
			if (holder && holder->data)
			{
				int cIndex = chunksTouched.size();
				chunksTouched.push_back(holder);
				auto nData = holder->data.get();

				for (int z = 0; z < CHUNK_SIZE; z++)
				{
					for (int x = 0; x < CHUNK_SIZE; x++)
					{
						int blockLight = data->GetLightLevel(x, 0, z);
						int nBlockLight = nData->GetLightLevel(x, CHUNK_SIZE - 1, z);
						int skyLight = data->GetSkyLightLevel(x, 0, z);
						int nSkyLight = nData->GetSkyLightLevel(x, CHUNK_SIZE - 1, z);
						if (nBlockLight > blockLight + 1)
							propagationQueue.emplace(ChunkData::Index(x, CHUNK_SIZE - 1, z), cIndex, PropInOneDir(POS_Y));
						if (nSkyLight > skyLight + 1)
							skyPropagationQueue.emplace(ChunkData::Index(x, CHUNK_SIZE - 1, z), cIndex, PropInOneDir(POS_Y));
					}
				}
			}

			// Positive Z
			holder = chunkManager->GetChunkHolder(data->GetId() + glm::ivec3(0, 0, 1));
			if (holder && holder->data)
			{
				int cIndex = chunksTouched.size();
				chunksTouched.push_back(holder);
				auto nData = holder->data.get();

				for (int x = 0; x < CHUNK_SIZE; x++)
				{
					for (int y = 0; y < CHUNK_SIZE; y++)
					{
						int blockLight = data->GetLightLevel(x, y, CHUNK_SIZE - 1);
						int nBlockLight = nData->GetLightLevel(x, y, 0);
						int skyLight = data->GetSkyLightLevel(x, y, CHUNK_SIZE - 1);
						int nSkyLight = nData->GetSkyLightLevel(x, y, 0);
						if (nBlockLight > blockLight + 1)
							propagationQueue.emplace(ChunkData::Index(x, y, 0), cIndex, PropInOneDir(NEG_Z));
						if (nSkyLight > skyLight + 1)
							skyPropagationQueue.emplace(ChunkData::Index(x, y, 0), cIndex, PropInOneDir(NEG_Z));
					}
				}
			}

			// Negative Z
			holder = chunkManager->GetChunkHolder(data->GetId() + glm::ivec3(0, 0, -1));
			if (holder && holder->data)
			{
				int cIndex = chunksTouched.size();
				chunksTouched.push_back(holder);
				auto nData = holder->data.get();

				for (int x = 0; x < CHUNK_SIZE; x++)
				{
					for (int y = 0; y < CHUNK_SIZE; y++)
					{
						int blockLight = data->GetLightLevel(x, y, 0);
						int nBlockLight = nData->GetLightLevel(x, y, CHUNK_SIZE - 1);
						int skyLight = data->GetSkyLightLevel(x, y, 0);
						int nSkyLight = nData->GetSkyLightLevel(x, y, CHUNK_SIZE - 1);
						if (nBlockLight > blockLight + 1)
							propagationQueue.emplace(ChunkData::Index(x, y, CHUNK_SIZE - 1), cIndex, PropInOneDir(POS_Z));
						if (nSkyLight > skyLight + 1)
							skyPropagationQueue.emplace(ChunkData::Index(x, y, CHUNK_SIZE - 1), cIndex, PropInOneDir(POS_Z));
					}
				}
			}

			PropagateLight<BlockChannel>(chunkManager, propagationQueue, chunksTouched);
			PropagateLight<SkyChannel>(chunkManager, skyPropagationQueue, chunksTouched);
			return chunksTouched;
		}
	}
}