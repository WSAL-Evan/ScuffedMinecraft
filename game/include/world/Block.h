#pragma once

#include <wv/pch.h>

namespace ScuffedMinecraft
{
	struct Block
	{
		std::string name;
		bool emitsLight;
		int lightLevel;

		Block(const std::string& name, bool emitsLight = false, int lightLevel = 0)
			: name(name), emitsLight(emitsLight), lightLevel(lightLevel) {}
	};
}