#pragma once

namespace ScuffedMinecraft
{
	struct BlockDisplay
	{
		float topTextureMinX, topTextureMaxX, topTextureMinY, topTextureMaxY;
		float bottomTextureMinX, bottomTextureMaxX, bottomTextureMinY, bottomTextureMaxY;
		float sideTextureMinX, sideTextureMaxX, sideTextureMinY, sideTextureMaxY;
		bool foliageRendering;

		BlockDisplay(float textureMinX, float textureMaxX, float textureMinY, float textureMaxY, bool foliageRendering = false)
			: topTextureMinX(textureMinX), topTextureMaxX(textureMaxX), topTextureMinY(textureMinY), topTextureMaxY(textureMaxY),
			bottomTextureMinX(textureMinX), bottomTextureMaxX(textureMaxX), bottomTextureMinY(textureMinY), bottomTextureMaxY(textureMaxY),
			sideTextureMinX(textureMinX), sideTextureMaxX(textureMaxX), sideTextureMinY(textureMinY), sideTextureMaxY(textureMaxY), foliageRendering(foliageRendering) {}

		BlockDisplay(float topTextureMinX, float topTextureMaxX, float topTextureMinY, float topTextureMaxY,
			float bottomTextureMinX, float bottomTextureMaxX, float bottomTextureMinY, float bottomTextureMaxY,
			float sideTextureMinX, float sideTextureMaxX, float sideTextureMinY, float sideTextureMaxY)
			: topTextureMinX(topTextureMinX), topTextureMaxX(topTextureMaxX), topTextureMinY(topTextureMinY), topTextureMaxY(topTextureMaxY),
			bottomTextureMinX(bottomTextureMinX), bottomTextureMaxX(bottomTextureMaxX), bottomTextureMinY(bottomTextureMinY), bottomTextureMaxY(bottomTextureMaxY),
			sideTextureMinX(sideTextureMinX), sideTextureMaxX(sideTextureMaxX), sideTextureMinY(sideTextureMinY), sideTextureMaxY(sideTextureMaxY), foliageRendering(false) {}
	};
}