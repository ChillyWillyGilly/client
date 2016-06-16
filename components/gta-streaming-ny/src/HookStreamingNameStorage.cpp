/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "Hooking.h"
#include "StreamingTypes.h"

static std::unordered_map<int, std::string> g_indexToName;

std::string GetStreamName(int idx, int typeIdx)
{
	return g_indexToName[(typeIdx << 24) | idx];
}

int GetTypeStart(int type)
{
	return streamingTypes.types[type].startIndex;
}

int GetTypeEnd(int type)
{
	return streamingTypes.types[type + 1].startIndex - 1;
}

extern char curImgLabel[256];

extern bool g_naviDestiny;

static int RegisterFileName(const char* name, int type)
{
	name = curImgLabel;

	// ignore default meshes if we're on viiv
	if (g_naviDestiny)
	{
		if (!_strnicmp(name, "sectors2x2", 10))
		{
			return -1;
		}
	}

	int typeIdx = type / 100;
	int modelIdx = streamingTypes.types[typeIdx].getIndexByName(name);

	trace("Registering model %s as %s #%d\n", name, streamingTypes.types[typeIdx].typeName, modelIdx);

	g_indexToName[(typeIdx << 24) | modelIdx] = name;

	return modelIdx;
}

static void __declspec(naked) StoreImgEntryNameStub()
{
	__asm
	{
		push esi
		push eax

		call RegisterFileName

		add esp, 8h

		retn
	}
}

static HookFunction hookFunction([] ()
{
	// img index entry add function, call to file handler registration
	hook::nop(0xBCC370, 6);
	hook::call(0xBCC370, StoreImgEntryNameStub);
});