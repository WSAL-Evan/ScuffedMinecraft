#pragma once

#include <world/ChunkData.h>
#include <world/WorldFeature.h>
#include <world/FeatureSnapshot.h>
#include <FastNoise/FastNoise.h>

namespace ScuffedMinecraft
{
	class WorldGen
	{
	public:
		WorldGen(float minHeight, float maxHeight, float surfaceScale,
			int surfaceOctaves, float surfaceGain, float surfaceLacunarity,
			float caveScale, int caveOctaves, float caveGain, float caveLacunarity, float caveThreshold,
			std::vector<WorldFeature>&& features);
		~WorldGen() = default;

		void GenerateChunkShape(const glm::ivec3& id, ChunkData* data);
		void GenerateChunkFeatures(const glm::ivec3& id, FeatureSnapshot* snapshot);

	private:
		FastNoise::SmartNode<FastNoise::FractalFBm> _surfaceNoise;
		FastNoise::SmartNode<FastNoise::FractalFBm> _caveNoise;
		float _caveThreshold;

		std::vector<WorldFeature> _features;
	};
}