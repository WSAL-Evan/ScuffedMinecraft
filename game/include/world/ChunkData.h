#pragma once

#include <world/WorldDefines.h>

namespace ScuffedMinecraft
{
	struct ChunkData
	{
	public:

		ChunkData(const glm::ivec3& id)
			: _id(id)
		{
			memset(_data, AIR, sizeof(_data));
			memset(_lightData, 0, sizeof(_lightData));
		}

		static inline bool InBounds(int x, int y, int z) noexcept
		{
			return 0 <= x && x < CHUNK_SIZE &&
				0 <= y && y < CHUNK_SIZE &&
				0 <= z && z < CHUNK_SIZE;
		}

		static inline bool InBounds(int index) noexcept
		{
			return index >= 0 && index < CHUNK_VOLUME;
		}

		static inline int Index(int x, int y, int z) noexcept
		{
			assert(x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE);
			return z << (CHUNK_SHIFT * 2) | x << CHUNK_SHIFT | y;
		}

		static inline glm::ivec3 IndexToPos(int index) noexcept
		{
			return { (index >> CHUNK_SHIFT) & CHUNK_MASK, index & CHUNK_MASK, (index >> (CHUNK_SHIFT * 2)) & CHUNK_MASK };
		}

		static inline int GetX(int index) noexcept
		{
			return (index >> CHUNK_SHIFT) & CHUNK_MASK;
		}

		static inline int GetY(int index) noexcept
		{
			return index & CHUNK_MASK;
		}

		static inline int GetZ(int index) noexcept
		{
			return (index >> (CHUNK_SHIFT * 2));
		}

		// 0 = invalid block position
		BlockId GetBlock(int x, int y, int z) const
		{
			assert(InBounds(x, y, z));
			int index = Index(x, y, z);
			return _data[index];
		}

		// 0 = invalid block position
		BlockId GetBlock(int index) const
		{
			assert(InBounds(index));
			return _data[index];
		}

		// 0 = invalid block position
		// Returns old block ID
		BlockId SetBlock(int x, int y, int z, int blockId)
		{
			assert(InBounds(x, y, z));
			int index = Index(x, y, z);
			BlockId old = _data[index];
			_data[index] = blockId;
			return old;
		}

		// 0 = invalid block position
		// Returns old block ID
		BlockId SetBlock(int index, int blockId)
		{
			assert(InBounds(index));
			BlockId old = _data[index];
			_data[index] = blockId;
			return old;
			
		}

		int GetSkyLightLevel(int index) const
		{
			// XXXX0000
			assert(InBounds(index));
			return (_lightData[index] >> 4) & 0xF;
		}

		int GetSkyLightLevel(int x, int y, int z) const
		{
			assert(InBounds(x, y, z));
			return GetSkyLightLevel(Index(x, y, z));
		}

		void SetSkyLightLevel(int index, int light)
		{
			// XXXX0000
			assert(InBounds(index));
			_lightData[index] = (_lightData[index] & 0xF) | (light << 4);
		}

		void SetSkyLightLevel(int x, int y, int z, int light)
		{
			assert(InBounds(x, y, z));
			SetSkyLightLevel(Index(x, y, z), light);
		}

		int GetLightLevel(int index) const
		{
			// 0000XXXX
			assert(InBounds(index));
			return _lightData[index] & 0xF;
		}

		int GetLightLevel(int x, int y, int z) const
		{
			assert(InBounds(x, y, z));
			return GetLightLevel(Index(x, y, z));
		}

		void SetLightLevel(int index, int light)
		{
			// 0000XXXX
			assert(InBounds(index));
			_lightData[index] = (_lightData[index] & 0xF0) | light;
		}

		void SetLightLevel(int x, int y, int z, int light)
		{
			assert(InBounds(x, y, z));
			SetLightLevel(Index(x, y, z), light);
		}

		uint8_t GetRawLightLevel(int index) const
		{
			assert(InBounds(index));
			return _lightData[index];
		}

		uint8_t GetRawLightLevel(int x, int y, int z) const
		{
			assert(InBounds(x, y, z));
			return _lightData[Index(x, y, z)];
		}

		glm::ivec3 GetId() const { return _id; }

		const std::vector<int>& GetSurfaceBlocks() const { return _surfaceBlocks; }
		void SetSurfaceBlocks(std::vector<int>&& surfaceBlocks) { _surfaceBlocks = std::move(surfaceBlocks); }
		void ClearSurfaceBlocks() { _surfaceBlocks = {}; }

		const BlockId* GetData() const { return _data; }
		const uint8_t* GetLightData() const { return _lightData; }

		void SetData(BlockId* data)
		{
			memcpy(_data, data, CHUNK_VOLUME * sizeof(BlockId));
		}

	private:
		glm::ivec3 _id;
		BlockId _data[CHUNK_VOLUME];
		uint8_t _lightData[CHUNK_VOLUME];

		std::vector<int> _surfaceBlocks;
	};
}