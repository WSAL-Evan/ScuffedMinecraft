#pragma once

#include <world/ChunkData.h>
#include <FastNoise/FastNoise.h>

namespace ScuffedMinecraft
{
	class WorldGen
	{
	public:
		WorldGen(float minHeight, float maxHeight, float surfaceScale, 
			int surfaceOctaves, float surfaceGain, float surfaceLacunarity,
			float caveScale, int caveOctaves, float caveGain, float caveLacunarity, float caveThreshold);
		~WorldGen() = default;

		void GenerateChunk(const glm::ivec3& id, ChunkData* data);

	private:
		FastNoise::SmartNode<FastNoise::FractalFBm> _surfaceNoise;
		FastNoise::SmartNode<FastNoise::FractalFBm> _caveNoise;
		float _caveThreshold;
	};
}