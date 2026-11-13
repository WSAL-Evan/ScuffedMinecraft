#pragma once

#include <wv/pch.h>

namespace ScuffedMinecraft
{
	struct Block
	{
		std::string name;
		bool transparent;
		int lightBlocked;
		bool emitsLight;
		int lightLevel;


		Block(const std::string& name, bool transparent = false, int lightBlocked = 0, bool emitsLight = false, int lightLevel = 0)
			: name(name), transparent(transparent), lightBlocked(lightBlocked), emitsLight(emitsLight), lightLevel(lightLevel) {}
	};
}