#pragma once
#include "Definitions.h"

namespace InfinideaStudio
{
	namespace NEWorld
	{
		enum { CHUNK_DATA = 2, CHUNK_COMMAND, CHUNK_EMPTY, CHUNK_NOTBEMODIFIED };
		struct ChunkPacket
		{
			int cx, cy, cz;
			block pblocks [4096];
			brightness pbrightness [4096];
		};

		enum { CHUNK_COMMAND_CHANGEBLOCK };
		struct ChunkCommand
		{
			int command;
			int cx, cy, cz;
			int x, y, z;
			int extraInfo;
		};
	}
}